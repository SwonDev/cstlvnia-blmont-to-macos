"""Prepara y construye la adaptación usando exclusivamente los datos propios aportados."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import venv

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "private"
DATA = PRIVATE / "castlevania"


def run(*command, **kwargs):
    subprocess.run([str(arg) for arg in command], check=True, cwd=ROOT, **kwargs)


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, encoding="utf-8", delete=False) as temporary:
        json.dump(value, temporary, ensure_ascii=False, indent=2)
        temporary.write("\n")
    os.replace(temporary.name, path)


def python_environment():
    destination = PRIVATE / "venv"
    python = destination / "bin/python"
    if not python.exists():
        venv.EnvBuilder(with_pip=True).create(destination)
    stamp = destination / ".requirements-sha256"
    wanted = digest(ROOT / "requirements.txt")
    if not stamp.exists() or stamp.read_text().strip() != wanted:
        run(python, "-m", "pip", "install", "--disable-pip-version-check", "--require-hashes", "-r", ROOT / "requirements.txt")
        stamp.write_text(wanted + "\n", encoding="utf-8")
    return python


def validate_input(exefs, romfs, icon, specification, load_module):
    exefs, romfs, icon = (Path(p).expanduser().resolve() for p in (exefs, romfs, icon))
    if not exefs.is_dir() or not romfs.is_dir() or not icon.is_file():
        raise ValueError("Faltan ExeFS, RomFS o el icono Control aportados")
    if not (romfs / "Data").is_dir():
        raise ValueError("RomFS debe contener Data; no indicar directamente su subcarpeta")
    modules = []
    for expected in specification["modules"]:
        path = exefs / expected["name"]
        with path.open("rb") as source:
            header = source.read(0x100)
        if len(header) != 0x100 or header[:4] != b"NSO0":
            raise ValueError(f"Módulo NSO inválido: {expected['name']}")
        if header[0x40:0x60].hex() != expected["build_id"]:
            raise ValueError(f"Build ID no compatible: {expected['name']}; se requiere contenido 1.1.0")
        module = load_module(str(path), expected["name"])
        if len(module.image) != expected["image_bytes"] or hashlib.sha256(module.image).hexdigest() != expected["image_sha256"]:
            raise ValueError(f"Imagen no compatible: {expected['name']}")
        modules.append((expected, module))
    return {"exefs": str(exefs), "romfs": str(romfs), "icon": str(icon)}, modules


def prepare_inputs(options, load_module):
    config_path = PRIVATE / "input.json"
    if options.exefs or options.romfs or options.icon:
        if not all((options.exefs, options.romfs, options.icon)):
            raise ValueError("La primera preparación requiere --exefs, --romfs y --icon juntos")
        config = {"exefs": options.exefs, "romfs": options.romfs, "icon": options.icon}
    elif config_path.exists():
        config = json.loads(config_path.read_text(encoding="utf-8"))
    else:
        raise ValueError("Aporta --exefs, --romfs y --icon de tu propia copia; consulta README.md")
    specification = json.loads((ROOT / "games/castlevania/build.json").read_text(encoding="utf-8"))
    config, modules = validate_input(**config, specification=specification, load_module=load_module)
    analysis = DATA / "analysis"
    analysis.mkdir(parents=True, exist_ok=True)
    for expected, module in modules:
        path = analysis / (expected["name"] + ".bin")
        if not path.exists() or digest(path) != expected["image_sha256"]:
            with tempfile.NamedTemporaryFile(dir=analysis, delete=False) as temporary:
                temporary.write(module.image)
            os.replace(temporary.name, path)
    romfs_link = DATA / "extract/romfs"
    romfs_link.parent.mkdir(parents=True, exist_ok=True)
    if romfs_link.is_symlink():
        if romfs_link.resolve() != Path(config["romfs"]):
            raise ValueError("Ya hay un RomFS distinto preparado; conservarlo y usar otro checkout")
    elif romfs_link.exists():
        raise ValueError("Ya existe un RomFS preparado que no es el enlace gestionado")
    else:
        romfs_link.symlink_to(config["romfs"], target_is_directory=True)
    atomic_json(config_path, config)
    (DATA / "language.txt").write_text(options.language + "\n", encoding="utf-8")
    return config, specification


def generator_fingerprint(root, specification):
    inputs = [root / "games/castlevania/resolution-patches.json", root / "requirements.txt",
              *sorted((root / "recomp").glob("*.py"))]
    recipe = {"schema": 1, "exceptions": True, "modules": specification["modules"],
              "sources": {path.relative_to(root).as_posix(): digest(path) for path in inputs}}
    return hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()


def generate(python, config, specification):
    fingerprint = generator_fingerprint(ROOT, specification)
    for module in specification["modules"]:
        destination = DATA / "gen" / module["name"]
        stamp = destination / ".generator-sha256"
        if stamp.exists() and stamp.read_text().strip() == fingerprint and (destination / "table.cpp").exists():
            print(f"AOT vigente: {module['name']}", flush=True)
            continue
        command = [python, "-m", "recomp", Path(config["exefs"]) / module["name"], destination,
                   "--name", module["name"], "--base", module["base"], "--exceptions"]
        if module["name"] == "main":
            command += ["--patches", ROOT / "games/castlevania/resolution-patches.json"]
        run(*command)
        stamp.write_text(fingerprint + "\n", encoding="utf-8")


def doctor():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise ValueError("La construcción del juego requiere macOS en Apple silicon")
    missing = [name for name in ("cmake", "ninja", "brew", "dotnet", "spirv-val", "codesign", "xcrun") if not shutil.which(name)]
    if missing:
        raise ValueError("Faltan herramientas: " + ", ".join(missing) + ". Consulta los requisitos del README.")
    for name in ("sdl3", "molten-vk", "vulkan-headers", "vulkan-loader"):
        subprocess.run(["brew", "--prefix", name], check=True, stdout=subprocess.DEVNULL)
    run("xcrun", "--find", "clang++")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exefs", type=Path)
    parser.add_argument("--romfs", type=Path)
    parser.add_argument("--icon", type=Path)
    parser.add_argument("--stage", choices=("prepare", "generate", "shaders", "build", "package", "test", "all"), default="all")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 4))
    parser.add_argument("--language", default="es", choices=("es", "en", "ja"))
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--install-dir", type=Path)
    parser.add_argument("--doctor", action="store_true")
    options = parser.parse_args()
    if sys.version_info < (3, 12):
        parser.error("Se requiere Python 3.12 o posterior")
    if not 1 <= options.jobs <= 32:
        parser.error("--jobs debe estar entre 1 y 32")
    os.umask(0o077)
    doctor()
    if options.doctor:
        print("Herramientas disponibles; esto no acredita los datos o la compilación.")
        return
    PRIVATE.mkdir(exist_ok=True)
    python = python_environment()
    if Path(sys.prefix).resolve() != python.parent.parent.resolve():
        os.execv(str(python), [str(python), str(Path(__file__).resolve()), *sys.argv[1:]])
    if str(ROOT) not in sys.path:
        sys.path.insert(0, str(ROOT))
    if options.stage == "test":
        run("cmake", "-S", ROOT, "-B", PRIVATE / "build-tests", "-G", "Ninja", "-DNX_TESTS=ON", "-DNX_BUILD_GAME=OFF")
        run("cmake", "--build", PRIVATE / "build-tests", "-j", options.jobs)
        run("ctest", "--test-dir", PRIVATE / "build-tests", "--output-on-failure")
        run(python, "-m", "unittest", "discover", "-s", "tests", "-p", "*test*.py")
        return
    from recomp.nso import load
    config, specification = prepare_inputs(options, load)
    if options.stage == "prepare":
        return
    if options.stage in ("generate", "build", "all"):
        generate(python, config, specification)
    if options.stage == "generate":
        return
    if options.stage in ("shaders", "all"):
        run(python, ROOT / "scripts/bootstrap_sources.py")
        run(python, ROOT / "scripts/prepare_shaders.py", "--data", DATA, "--jobs", options.jobs)
    if options.stage == "shaders":
        return
    build = PRIVATE / "build-macos"
    if options.stage in ("build", "all"):
        run("cmake", "-S", ROOT, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DNX_GAME=castlevania", "-DNX_TESTS=ON", "-DNX_BUILD_GAME=ON", f"-DNX_GEN_DIR={DATA / 'gen'}")
        run("cmake", "--build", build, "-j", options.jobs)
        run("ctest", "--test-dir", build, "--output-on-failure")
        run(python, "-m", "unittest", "discover", "-s", "tests", "-p", "*test*.py")
        if options.stage == "build":
            return
    command = [python, ROOT / "scripts/macos_bundle.py", "--binary", build / "castlevania", "--data", DATA,
               "--icon", config["icon"], "--output", PRIVATE / "dist/Castlevania Belmont’s Curse.app"]
    if options.install or options.install_dir:
        install_dir = (options.install_dir or Path.home() / "Applications").expanduser()
        install_dir.mkdir(parents=True, exist_ok=True)
        command += ["--install-dir", install_dir]
    run(*command)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
