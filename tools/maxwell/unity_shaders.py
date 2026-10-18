"""Extrae bloques NVN de shaders Unity; conserva los recursos derivados en private/."""

import argparse
import json
from pathlib import Path
import struct

from .containers import Container, Stage, SPH_TYPE, extract


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


def main():
    import UnityPy
    from UnityPy.helpers import CompressionHelper

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--compute-only", action="store_true", help="Inspeccionar sólo ComputeShader")
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[2] / "private"
    if not args.out.resolve().is_relative_to(private.resolve()):
        parser.error("La salida debe estar dentro de private/")
    args.out.mkdir(parents=True, exist_ok=True)
    paths = sorted(args.data.rglob("*.bundle")) + sorted(args.data.glob("*.assets"))
    report = {"unitypy": UnityPy.__version__, "archivos": len(paths), "shaders": 0, "compute": 0, "etapas": 0, "sin_programas": [], "errores": []}
    hashes = set()
    for number, path in enumerate(paths):
        try:
            environment = UnityPy.load(str(path))
            for obj in environment.objects:
                identity = str(obj.path_id)
                destination = args.out / f"{number}-{identity}"
                if obj.type.name == "ComputeShader":
                    report["compute"] += 1
                    stages, rejected = store_blob(obj.get_raw_data(), identity, destination)
                    report["errores"].extend([path.name, obj.path_id, offset, detail] for offset, detail in rejected)
                    if not stages:
                        report["errores"].append([path.name, obj.path_id, "ComputeShader sin bloques NVN reconocidos"])
                    report["etapas"] += len(stages)
                    hashes.update(stage.hash for stage in stages)
                    continue
                if obj.type.name != "Shader" or args.compute_only:
                    continue
                shader = obj.read()
                report["shaders"] += 1
                for platform, offsets, sizes, decoded_sizes in zip(shader.platforms, shader.offsets, shader.compressedLengths, shader.decompressedLengths, strict=True):
                    if platform != 19:
                        report["errores"].append([path.name, obj.path_id, f"Plataforma de shader inesperada: {platform}"])
                        continue
                    if isinstance(offsets, int):
                        offsets, sizes, decoded_sizes = [offsets], [sizes], [decoded_sizes]
                    compressed = bytes(shader.compressedBlob)
                    chunks = []
                    for offset, size, decoded_size in zip(offsets, sizes, decoded_sizes, strict=True):
                        if offset < 0 or size < 0 or offset + size > len(compressed):
                            raise ValueError("Intervalo comprimido fuera del shader")
                        chunks.append(CompressionHelper.decompress_lz4(compressed[offset:offset + size], decoded_size))
                    blob = b"".join(chunks)
                    if blob == b"\0\0\0\0":
                        report["sin_programas"].append([path.name, obj.path_id, shader.m_ParsedForm.m_Name])
                        continue
                    stages, rejected = store_blob(blob, identity, destination)
                    report["errores"].extend([path.name, obj.path_id, offset, detail] for offset, detail in rejected)
                    if not stages:
                        report["errores"].append([path.name, obj.path_id, "Shader Switch sin bloques NVN reconocidos"])
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
