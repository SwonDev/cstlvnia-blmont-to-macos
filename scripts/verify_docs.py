"""Comprueba inventario, enlaces locales e identidad de capturas; no certifica privacidad."""

import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import zlib


def png_dimensions(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Firma PNG inválida")
    pos = 8
    kinds = []
    dimensions = None
    while pos < len(data):
        if pos + 12 > len(data):
            raise ValueError("PNG truncado")
        size = struct.unpack_from(">I", data, pos)[0]
        end = pos + 12 + size
        if end > len(data):
            raise ValueError("Chunk PNG truncado")
        kind = data[pos + 4:pos + 8]
        payload = data[pos + 8:end - 4]
        crc = struct.unpack_from(">I", data, end - 4)[0]
        if zlib.crc32(kind + payload) != crc:
            raise ValueError("CRC PNG inválido")
        if kind not in {b"IHDR", b"IDAT", b"IEND"}:
            raise ValueError("Chunk no aprobado: revisar metadatos de la captura")
        if kind == b"IHDR":
            if kinds or size != 13:
                raise ValueError("IHDR inválido")
            dimensions = struct.unpack_from(">II", payload)
        kinds.append(kind)
        pos = end
        if kind == b"IEND":
            if size or pos != len(data):
                raise ValueError("Contenido después de IEND")
            break
    if dimensions is None or not all(dimensions) or b"IDAT" not in kinds or not kinds or kinds[-1] != b"IEND":
        raise ValueError("PNG incompleto")
    return dimensions


def local_target(root, source, target):
    path = (source.parent / target.split("#", 1)[0]).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError("Enlace local fuera del repositorio o inexistente")
    return path


def verify(root):
    allowed = json.loads((root / "PUBLIC_FILES.json").read_text(encoding="utf-8"))
    if len(allowed) != len(set(allowed)):
        raise ValueError("Inventario duplicado")
    actual = set()
    for directory, folders, files in os.walk(root, followlinks=False):
        base = Path(directory)
        folders[:] = [name for name in folders if name not in {".git", "__pycache__"}
                      and not (base == root and name == "private")]
        for name in folders + files:
            path = base / name
            if path.is_symlink():
                raise ValueError("Enlace simbólico no aprobado")
            if path.is_file():
                actual.add(path.relative_to(root).as_posix())
    if actual != set(allowed):
        raise ValueError("El árbol difiere de PUBLIC_FILES.json; revisar inventario")
    links = 0
    for name in allowed:
        path = root / name
        if path.stat().st_size > 4 * 1024 * 1024:
            raise ValueError("Archivo supera el presupuesto de 4 MiB")
        if path.suffix == ".png":
            continue
        content = path.read_text(encoding="utf-8")
        if path.suffix == ".json":
            json.loads(content)
        if path.suffix == ".md":
            for target in re.findall(r"!?\[[^\]]*\]\(([^\s)]+)\)", content):
                if re.match(r"https?://|#", target):
                    continue
                local_target(root, path, target)
                links += 1
    manifest_path = root / "assets/screenshots/manifest.json"
    images = json.loads(manifest_path.read_text(encoding="utf-8"))["screenshots"] if manifest_path.exists() else []
    names = [item["file"] for item in images]
    if len(names) != len(set(names)) or set(names) != {name for name in allowed if name.endswith(".png")}:
        raise ValueError("Manifiesto de capturas incompleto o duplicado")
    for item in images:
        data = (root / item["file"]).read_bytes()
        if len(data) != item["bytes"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise ValueError("La captura difiere del manifiesto revisado")
        if png_dimensions(data) != (item["width"], item["height"]):
            raise ValueError("Dimensiones de captura incorrectas")
    return len(actual), links, len(images)


if __name__ == "__main__":
    try:
        files, links, images = verify(Path(__file__).resolve().parents[1])
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(f"FALLO: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"PASS: {files} archivos, {links} enlaces locales, {images} PNG con identidad y chunks revisados.")
    print("Requiere además revisión humana de contenido, derechos y privacidad.")
