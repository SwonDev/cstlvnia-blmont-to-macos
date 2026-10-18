#!/usr/bin/env python3
"""Genera shaders desde ExeFS plano y RomFS propios, sin importar una caché previa."""

from contextlib import contextmanager
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.maxwell.containers import Stage
from tools.maxwell.unity_shaders import scan_blob


def scan_embedded(blob, identity):
    """Reconoce parejas control/código contiguas y compute sin prefijo NVN."""
    stages, rejected = scan_blob(blob, identity)
    cursor = 0
    while (offset := blob.find(b'\x34\x12\x76\x98', cursor)) >= 0:
        cursor = offset + 4
        code = offset + 0x880
        if offset % 4 or code > len(blob):
            continue
        length, const_size, const_off, size, start = struct.unpack_from('<5I', blob, offset + 0x6F8)
        x, y, z = struct.unpack_from('<3I', blob, offset + 0x748)
        if start != 0 or not 0 < length <= size <= len(blob) - code or length % 8:
            continue
        if not all(0 < n <= 1024 for n in (x, y, z)) or x * y * z > 1024:
            continue
        program = blob[code:code + length]
        if struct.pack('<Q', 0xE30000000007000F) not in program or not any(program[-8:]):
            continue
        stage = Stage(f'{identity}/{offset:x}', 'compute', code, size, offset, 0x880, blob)
        problems = [problem for problem in stage.problems if not problem.startswith('code magic ')]
        if problems:
            continue
        stage.problems = []
        stages.append(stage)
        rejected = [(pos, detail) for pos, detail in rejected if pos != offset]
    return stages, rejected


class ShaderInventory:
    """Deduplica código y comprueba el estado de control que consume el traductor."""

    def __init__(self, destination):
        self.destination = Path(destination)
        self.records = {}

    def add(self, blob, stage, origin, required=False):
        import hashlib
        code = bytes(blob[stage.code_off:stage.code_off + stage.code_size])
        ctrl = bytes(blob[stage.ctrl_off:stage.ctrl_off + stage.ctrl_size])
        semantics = (ctrl[0x6F8:0x70C] + ctrl[0x748:0x758]).hex()
        sha = hashlib.sha256(code).hexdigest()
        control_sha = hashlib.sha256(ctrl).hexdigest()
        source = {'origin': origin, 'id': stage.id, 'code_offset': stage.code_off,
                  'control_offset': stage.ctrl_off, 'control_sha256': control_sha}
        previous = self.records.get(stage.hash)
        if previous:
            if previous['sha256'] != sha or previous['stage'] != stage.kind or previous['control_fields'] != semantics:
                raise ValueError(f'{stage.hash}: código o control incompatible para un hash duplicado')
            previous['sources'].append(source)
            previous['required'] |= required
            return
        folder = self.destination / stage.hash
        folder.mkdir(parents=True)
        (folder / 'code.bin').write_bytes(code)
        (folder / 'control.bin').write_bytes(ctrl)
        self.records[stage.hash] = {'hash': stage.hash, 'sha256': sha, 'stage': stage.kind,
            'control_fields': semantics, 'bytes': len(code), 'required': required, 'sources': [source],
            'code': str(folder / 'code.bin'), 'control': str(folder / 'control.bin')}


RESULT_FIELDS = ('hash', 'group', 'stage', 'stages_using', 'status', 'ms', 'spv_bytes',
                 'resources', 'translator_messages', 'error', 'draw_time_queries')


def consolidate_results(records, rows, output):
    """Comprueba cobertura y archivos antes de construir el TSV consumido por el runtime."""
    import csv
    by_hash = {}
    for row in rows:
        identity = row['hash']
        if identity in by_hash:
            raise ValueError(f'Resultado duplicado: {identity}')
        if identity not in records:
            raise ValueError(f'Resultado sin entrada: {identity}')
        by_hash[identity] = dict(row)
        if row['status'] in {'ok', 'ok-retry'}:
            binary = output / (identity + '.spv')
            if not binary.is_file() or binary.stat().st_size != int(row['spv_bytes']):
                raise ValueError(f'SPIR-V ausente o tamaño incoherente: {identity}')
            if binary.read_bytes()[:4] != b'\x03\x02\x23\x07':
                raise ValueError(f'SPIR-V sin cabecera: {identity}')
    missing = set(records) - by_hash.keys()
    if missing:
        raise ValueError(f'Faltan resultados para {len(missing)} programas')
    failed = {h for h, row in by_hash.items() if row['status'] not in {'ok', 'ok-retry'}}
    required = sorted(h for h in failed if records[h]['required'])
    additional = sorted(failed - set(required))
    with (output / 'all-results.tsv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, RESULT_FIELDS, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows({key: by_hash[h].get(key, '') for key in RESULT_FIELDS} for h in sorted(by_hash))
    with (output / 'results.tsv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, RESULT_FIELDS, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows({key: by_hash[h].get(key, '') for key in RESULT_FIELDS} for h in sorted(by_hash) if h not in failed)
    return {'programs': len(records), 'required': sum(r['required'] for r in records.values()),
            'valid': len(records) - len(failed), 'required_valid': not required,
            'required_failures': required, 'additional_failures': additional}


# Estos programas se observaron en el juego y están ausentes del antiguo glob de assets/bundles.
REQUIRED_BUILTINS = frozenset({'f1e35748cc0b35c2', '94102ea079cefdb7', 'bcbd6b1d33cbcbe3',
                               '3276ddaa4525c66e', '8f8cb0951070060d'})
SAMPLER_3D = '8aa7c2a144799b6f'


def extract_inputs(data, workspace):
    from tools.maxwell.unity_shaders import discover_unity_files, shader_blobs
    inventory = ShaderInventory(workspace / 'extracted')
    diagnostics = []
    empty = []
    root = data / 'extract' / 'romfs'
    if not root.is_dir():
        raise ValueError('Falta extract/romfs con los datos propios')
    paths = discover_unity_files(root)
    if not paths:
        raise ValueError('No se encontraron archivos Unity')
    for number, path in enumerate(paths):
        origin = path.relative_to(root).as_posix()
        for identity, kind, name, blob in shader_blobs(path):
            if blob == bytes(4):
                empty.append({'origin': origin, 'id': identity, 'name': name, 'kind': kind})
                continue
            stages, rejected = scan_blob(blob, identity)
            if rejected or not stages:
                raise ValueError(f'{origin}, objeto {identity}: bloques NVN no válidos ({rejected})')
            for stage in stages:
                inventory.add(blob, stage, origin, path.suffix in {'.bundle', '.assets'} or stage.hash in REQUIRED_BUILTINS)
        if number % 250 == 0:
            print(f'Unity {number}/{len(paths)}; {len(inventory.records)} programas únicos', flush=True)
    for module in ('main', 'sdk', 'subsdk0'):
        image = data / 'analysis' / (module + '.bin')
        blob = image.read_bytes()
        stages, rejected = scan_embedded(blob, module)
        diagnostics.extend({'module': module, 'offset': offset, 'detail': detail} for offset, detail in rejected)
        for stage in stages:
            inventory.add(blob, stage, 'analysis/' + module + '.bin', required=True)
    missing = (REQUIRED_BUILTINS | {SAMPLER_3D}) - inventory.records.keys()
    if missing:
        raise ValueError(f'Faltan programas internos necesarios: {sorted(missing)}')
    return inventory, {'unity_files': len(paths), 'empty_objects': empty, 'embedded_rejected': diagnostics}


def translation_plan(records):
    groups = {'2d': [], '3d': []}
    unsupported = []
    for identity, record in sorted(records.items()):
        if record['stage'] not in {'vertex', 'pixel', 'compute'}:
            unsupported.append(identity)
        else:
            groups['3d' if identity == SAMPLER_3D else '2d'].append(identity)
    return groups, unsupported


def translate_inventory(inventory, workspace, harness, jobs, spirv_val):
    import csv
    import subprocess
    import shutil
    records = inventory.records
    groups, unsupported = translation_plan(records)
    output = workspace / 'translated'
    output.mkdir()
    rows = []
    fields = ('id', 'stage', 'code_off', 'code_size', 'ctrl_off', 'ctrl_size', 'program_start',
              'program_bytes', 'const_off', 'const_size', 'code_sha1_16', 'code_file', 'ctrl_file')
    for sampler, identities in groups.items():
        if not identities:
            continue
        manifest_root = workspace / ('batch-' + sampler)
        manifest_root.mkdir()
        with (manifest_root / 'manifest.tsv').open('w', encoding='utf-8', newline='') as stream:
            writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
            writer.writeheader()
            for identity in identities:
                record = records[identity]
                writer.writerow({'id': identity, 'stage': record['stage'], 'code_sha1_16': identity,
                    'code_file': record['code'], 'ctrl_file': record['control']})
        destination = workspace / ('translated-' + sampler)
        command = ['dotnet', str(harness), '--batch', str(manifest_root), '--out', str(destination),
                   '--jobs', str(jobs), '--target-env', 'vulkan1.2', '--spirv-val', str(spirv_val),
                   '--sampler-type', sampler]
        print(f'Traducción {sampler}: {len(identities)} programas; validación vulkan1.2', flush=True)
        with (workspace / ('translator-' + sampler + '.log')).open('w', encoding='utf-8') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        with (destination / 'results.tsv').open(encoding='utf-8', newline='') as stream:
            batch_rows = list(csv.DictReader(stream, delimiter='\t'))
        for row in batch_rows:
            identity = row['hash']
            if identity not in identities:
                raise ValueError(f'Resultado fuera del lote: {identity}')
            expected_stage = {'pixel': 'fragment'}.get(records[identity]['stage'], records[identity]['stage'])
            if row['stage'] != expected_stage:
                raise ValueError(f'Etapa del traductor incoherente: {identity}')
            row['stages_using'] = str(len(records[identity]['sources']))
            row['group'] = 'own-inputs/' + sampler
            if row['status'] in {'ok', 'ok-retry'}:
                shutil.copy2(destination / (identity + '.spv'), output / (identity + '.spv'))
            rows.append(row)
    for identity in unsupported:
        rows.append({'hash': identity, 'group': 'own-inputs/unsupported', 'stage': records[identity]['stage'],
            'stages_using': str(len(records[identity]['sources'])), 'status': 'unsupported', 'spv_bytes': '0',
            'error': 'Etapa no soportada por el accesor estático macOS; no traducida como vertex'})
    return output, rows


REQUIRED_SET_SHA256 = '9aadafce2409b2fbee5d00812f0808f4224bd3e47e1c794b3fbd77a4b4512bfe'


def verify_required_coverage(records, expected=REQUIRED_SET_SHA256):
    import hashlib
    identities = sorted(h for h, r in records.items() if r['required'])
    actual = hashlib.sha256(('\n'.join(identities) + '\n').encode('ascii')).hexdigest()
    if actual != expected:
        raise ValueError(f'Cobertura requerida distinta de Castlevania 1.1.0: {len(identities)} programas, SHA256 {actual}')
    return actual


@contextmanager
def harness_lock(path):
    """El obj/bin compartido no puede reconstruirse mientras otro proceso lo ejecuta."""
    import fcntl
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a', encoding='utf-8') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lock, fcntl.LOCK_UN)


def prepare(data, jobs):
    import json
    import shutil
    import subprocess
    import tempfile
    import os
    private = (ROOT / 'private').resolve()
    data = Path(data).resolve()
    if not data.is_relative_to(private) or data == private:
        raise ValueError('--data debe estar dentro de private/')
    if jobs < 1:
        raise ValueError('--jobs debe ser positivo')
    if (data / 'spirv').exists():
        raise ValueError('Ya existe spirv; conservarlo y usar una carpeta de datos nueva para regenerar')
    for name in ('main', 'sdk', 'subsdk0'):
        if not (data / 'analysis' / (name + '.bin')).is_file():
            raise ValueError(f'Falta analysis/{name}.bin original')
    if not (private / 'third_party/ryubing/src/Spv.Generator/Spv.Generator.csproj').is_file():
        raise ValueError('Faltan las fuentes fijadas de Ryubing; ejecutar bootstrap_sources.py')
    validator = shutil.which('spirv-val')
    if not validator:
        raise ValueError('No se encuentra spirv-val en PATH')
    workspace = Path(tempfile.mkdtemp(prefix='shader-preparation-', dir=data))
    print(f'Evidencias y derivados: {workspace}', flush=True)
    inventory, extraction = extract_inputs(data, workspace)
    coverage_sha = verify_required_coverage(inventory.records)
    (workspace / 'inventory.json').write_text(json.dumps({'extraction': extraction,
        'programs': list(inventory.records.values())}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Inventario: {len(inventory.records)} únicos; {sum(r["required"] for r in inventory.records.values())} necesarios', flush=True)
    print('Esperando acceso exclusivo al harness compartido', flush=True)
    with harness_lock(private / 'build/spirv_harness/prepare.lock'):
        from scripts.bootstrap_sources import verify_checkout
        verify_checkout(ROOT)
        with (workspace / 'build-harness.log').open('w', encoding='utf-8') as log:
            subprocess.run(['dotnet', 'build', str(ROOT / 'tools/maxwell/spirv_harness/SpirvHarness.csproj'),
                            '-c', 'Release', '-p:RestoreSources=https://api.nuget.org/v3/index.json'], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        harness = private / 'build/spirv_harness/bin/Release/net10.0/spirv_harness.dll'
        output, rows = translate_inventory(inventory, workspace, harness, jobs, validator)
    report = consolidate_results(inventory.records, rows, output)
    report.update({'workspace': str(workspace), 'required_set_sha256': coverage_sha, 'coverage': 'inventario estático; adicionales sin uso GUI acreditado',
                   'validation': 'spirv-val --target-env vulkan1.2', 'sampler3d': SAMPLER_3D})
    (workspace / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False), flush=True)
    if not report['required_valid']:
        return 1
    # Publicar sólo el conjunto validado; el informe completo conserva todos los límites y rechazos.
    candidate = workspace / 'runtime-spirv'
    candidate.mkdir()
    shutil.copy2(output / 'results.tsv', candidate / 'results.tsv')
    for row in rows:
        if row['status'] in {'ok', 'ok-retry'}:
            shutil.copy2(output / (row['hash'] + '.spv'), candidate / (row['hash'] + '.spv'))
    os.rename(candidate, data / 'spirv')
    return 0


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', required=True, type=Path)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    try:
        return prepare(args.data, args.jobs)
    except (ValueError, OSError) as error:
        parser.exit(1, f'Preparación de shaders: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
