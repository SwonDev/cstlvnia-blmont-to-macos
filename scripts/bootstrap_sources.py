"""Obtiene únicamente el traductor MIT fijado; nunca descarga datos del juego."""

import hashlib
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REVISION = "a82350bb774f70fcbd41c9987bf67a3775409963"
URL = "https://codeberg.org/Software_arch/Ryubing/archive/a82350bb77.tar.gz"
SHA256 = "aba9789bdcf3fda9e90f965636e9e85e9dcc299333fbf5451bcf22ecfd608506"
MAX_BYTES = 32 * 1024 * 1024


def checked_archive(archive):
    if not archive.is_file() or archive.stat().st_size > MAX_BYTES:
        raise ValueError("Archivo del traductor ausente o demasiado grande")
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise ValueError("El archivo del traductor no coincide con el SHA-256 fijado")


def verify_checkout(root):
    """Contrasta las fuentes consumidas contra el archivo fijado, sin fiarse del marcador."""
    destination = root / "private/third_party/ryubing"
    archive = destination.parent / "ryubing-a82350bb77.tar.gz"
    checked_archive(archive)
    prefixes = ("src/Ryujinx.Common/", "src/Spv.Generator/", "src/Ryujinx.Graphics.Shader/")
    expected = set()
    with tarfile.open(archive) as source:
        for member in source:
            relative = member.name.partition("/")[2]
            if not member.isfile() or not (relative.startswith(prefixes) or "/" not in relative):
                continue
            expected.add(relative)
            path = destination / relative
            if path.is_symlink() or not path.is_file() or path.read_bytes() != source.extractfile(member).read():
                raise ValueError(f"Fuente del traductor modificada o ausente: {relative}")
    for prefix in prefixes:
        for path in (destination / prefix).rglob("*"):
            relative = path.relative_to(destination)
            if set(relative.parts) & {"bin", "obj"}:
                continue
            if path.is_symlink() or (path.is_file() and relative.as_posix() not in expected):
                raise ValueError(f"Fuente adicional no fijada: {relative}")
    for path in destination.iterdir():
        if path.is_symlink() or (path.is_file() and path.name not in expected and path.name != ".port-revision"):
            raise ValueError(f"Archivo raíz adicional no fijado: {path.name}")
    if not expected:
        raise ValueError("Archivo del traductor sin fuentes reconocidas")
    return destination


def extract_source(archive, destination):
    checked_archive(archive)
    if destination.exists():
        raise ValueError("Ya existe otra fuente del traductor; se conserva")
    with tempfile.TemporaryDirectory(prefix="ryubing-", dir=destination.parent) as temporary:
        with tarfile.open(archive) as source:
            source.extractall(temporary, filter="data")
        children = list(Path(temporary).iterdir())
        if len(children) != 1 or not children[0].is_dir():
            raise ValueError("Estructura inesperada del archivo del traductor")
        candidate = children[0]
        for name in ("LICENSE.txt", "src/Ryujinx.Common/Ryujinx.Common.csproj", "src/Spv.Generator/Spv.Generator.csproj", "src/Ryujinx.Graphics.Shader/Ryujinx.Graphics.Shader.csproj"):
            if not (candidate / name).is_file():
                raise ValueError("Falta un componente del traductor fijado")
        shutil.move(candidate, destination)
    (destination / ".port-revision").write_text(REVISION + "\n", encoding="utf-8")


def main():
    parent = ROOT / "private/third_party"
    parent.mkdir(parents=True, exist_ok=True)
    destination = parent / "ryubing"
    stamp = destination / ".port-revision"
    if stamp.exists() and stamp.read_text().strip() == REVISION:
        verify_checkout(ROOT)
        print("Traductor fijado ya preparado.")
        return
    if destination.exists():
        raise ValueError("Fuente existente sin identidad compatible; no se sobrescribe")
    archive = parent / "ryubing-a82350bb77.tar.gz"
    if not archive.exists():
        with tempfile.NamedTemporaryFile(dir=parent, delete=False) as temporary:
            pending = Path(temporary.name)
            try:
                with urllib.request.urlopen(URL, timeout=60) as response:
                    size = 0
                    while block := response.read(1024 * 1024):
                        size += len(block)
                        if size > MAX_BYTES:
                            raise ValueError("Descarga del traductor supera el límite fijado")
                        temporary.write(block)
                temporary.flush()
                if hashlib.sha256(pending.read_bytes()).hexdigest() != SHA256:
                    raise ValueError("La descarga del traductor no coincide con su SHA-256")
                pending.rename(archive)
            finally:
                pending.unlink(missing_ok=True)
    extract_source(archive, destination)
    verify_checkout(ROOT)
    print("Traductor MIT y Spv.Generator preparados desde la revisión fijada.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, tarfile.TarError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
