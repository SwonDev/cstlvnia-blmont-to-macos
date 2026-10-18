"""Extrae bloques NVN de shaders Unity; conserva los recursos derivados en private/."""

import argparse
import json
from pathlib import Path
import struct

from .containers import Container, Stage, SPH_TYPE, extract


def discover_unity_files(root):
    """Incluye archivos serializados Unity aunque no tengan extensión."""
    result = []
    for path in sorted(Path(root).rglob("*")):
        if not path.is_file():
            continue
        with path.open("rb") as source:
            header = source.read(64)
        if path.suffix in {".bundle", ".assets"} or header.startswith((b"UnityFS\0", b"UnityRaw\0", b"UnityWeb\0", b"UnityWebData")):
            result.append(path)
            continue
        if len(header) < 48:
            continue
        metadata, size, version, offset = struct.unpack_from(">4I", header)
        if version >= 22:
            metadata, size, offset = struct.unpack_from(">IQQ", header, 20)
        if 5 <= version <= 100 and 0 < metadata <= size == path.stat().st_size and 20 <= offset <= size:
            result.append(path)
    return result


def scan_blob(blob, identity):
    stages, rejected = [], []
    cursor = 0
    while (offset := blob.find(b"\x34\x12\x76\x98", cursor)) >= 0:
        cursor = offset + 4
        code_offset = offset + 0x880
        if code_offset + 0x34 > len(blob):
            rejected.append((offset, "Control o código truncado"))
            continue
        code_size = struct.unpack_from("<I", blob, offset + 0x704)[0]
        magic = struct.unpack_from("<I", blob, code_offset)[0]
        if magic not in (0x12345678, 0x12345679) or not 0x34 <= code_size <= len(blob) - code_offset:
            rejected.append((offset, "Tamaño o cabecera NVN incompatible"))
            continue
        kind = "compute" if magic == 0x12345679 else SPH_TYPE.get((struct.unpack_from("<I", blob, code_offset + 0x30)[0] >> 10) & 15)
        if not kind:
            rejected.append((offset, "Etapa gráfica desconocida"))
            continue
        stage = Stage(f"{identity}/{offset:x}", kind, code_offset, code_size, offset, 0x880, blob)
        if stage.problems:
            rejected.append((offset, stage.problems))
        else:
            stages.append(stage)
    return stages, rejected


def store_blob(blob, identity, outdir):
    stages, rejected = scan_blob(blob, identity)
    container = Container("Unity", identity, 0, len(blob))
    container.programs = [(identity, stages)]
    extract(container, str(outdir), blob, unique=True)
    return stages, rejected


def shader_blobs(path, compute_only=False):
    """Devuelve objetos Shader/ComputeShader y sus blobs Switch descomprimidos."""
    import UnityPy
    from UnityPy.helpers import CompressionHelper

    for obj in UnityPy.load(str(path)).objects:
        identity = str(obj.path_id)
        if obj.type.name == "ComputeShader":
            tree = obj.read_typetree()
            variants = tree.get("variants", [])
            empty = bool(variants) and all(not variant.get("kernels") for variant in variants)
            yield identity, "compute", tree.get("m_Name", "ComputeShader"), bytes(4) if empty else obj.get_raw_data()
            continue
        if obj.type.name != "Shader" or compute_only:
            continue
        shader = obj.read()
        for platform, offsets, sizes, decoded_sizes in zip(shader.platforms, shader.offsets, shader.compressedLengths, shader.decompressedLengths, strict=True):
            if platform != 19:
                raise ValueError(f"Plataforma de shader inesperada: {platform}")
            if isinstance(offsets, int):
                offsets, sizes, decoded_sizes = [offsets], [sizes], [decoded_sizes]
            compressed = bytes(shader.compressedBlob)
            chunks = []
            for offset, size, decoded_size in zip(offsets, sizes, decoded_sizes, strict=True):
                if offset < 0 or size < 0 or decoded_size < 0 or offset + size > len(compressed):
                    raise ValueError("Intervalo comprimido fuera del shader")
                chunk = CompressionHelper.decompress_lz4(compressed[offset:offset + size], decoded_size)
                if len(chunk) != decoded_size:
                    raise ValueError("Tamaño descomprimido incoherente")
                chunks.append(chunk)
            yield identity, "shader", shader.m_ParsedForm.m_Name, b"".join(chunks)


def main():
    import UnityPy

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--compute-only", action="store_true", help="Inspeccionar sólo ComputeShader")
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[2] / "private"
    if not args.out.resolve().is_relative_to(private.resolve()):
        parser.error("La salida debe estar dentro de private/")
    args.out.mkdir(parents=True, exist_ok=True)
    paths = discover_unity_files(args.data)
    report = {"unitypy": UnityPy.__version__, "archivos": len(paths), "shaders": 0, "compute": 0, "etapas": 0, "sin_programas": [], "errores": []}
    hashes = set()
    for number, path in enumerate(paths):
        try:
            for identity, kind, name, blob in shader_blobs(path, args.compute_only):
                report["compute" if kind == "compute" else "shaders"] += 1
                if blob == b"\0\0\0\0":
                    report["sin_programas"].append([path.name, identity, name])
                    continue
                stages, rejected = store_blob(blob, identity, args.out / f"{number}-{identity}")
                report["errores"].extend([path.name, identity, offset, detail] for offset, detail in rejected)
                if not stages:
                    report["errores"].append([path.name, identity, "Shader Switch sin bloques NVN reconocidos"])
                report["etapas"] += len(stages)
                hashes.update(stage.hash for stage in stages)
        except Exception as error:
            report["errores"].append([path.name, type(error).__name__, str(error)])
        if number % 100 == 0:
            print(f"{number}/{len(paths)} archivos; {report['etapas']} etapas; {len(report['errores'])} errores", flush=True)
    report["programas_distintos"] = len(hashes)
    (args.out / "inventario.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({key: len(value) if isinstance(value, list) else value for key, value in report.items()}, ensure_ascii=False))
    return 1 if report["errores"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
