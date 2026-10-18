#!/usr/bin/env python3
"""Empaqueta localmente Castlevania con recursos propios; no descarga ni publica datos."""
from __future__ import annotations

import argparse
import csv
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

VERSION = '1.1.4'
IDENTIFIER = 'dev.swondev.castlevania'
LIBRARIES = {'sdl3': 'libSDL3.dylib', 'molten-vk': 'libMoltenVK.dylib'}


class BundleError(Exception):
    """Una entrada o verificación impide crear un paquete autónomo."""


def run(arguments: list[str]) -> str:
    return subprocess.run(arguments, check=True, capture_output=True, text=True).stdout


def no_symlinks(path: Path) -> None:
    for part in (path, *path.parents):
        if part.is_symlink():
            raise BundleError('No se admiten destinos ni antecesores simbólicos')


def absolute(path: Path) -> Path:
    return Path(os.path.abspath(path.expanduser()))


def output_path(root: Path, path: Path) -> Path:
    target = absolute(path)
    no_symlinks(target)
    private = root.resolve() / 'private'
    if target.suffix != '.app' or not target.is_relative_to(private):
        raise BundleError('El destino debe ser una .app nueva dentro de private/')
    if target.exists():
        raise BundleError('El destino ya existe; no se reemplaza')
    return target


def require_file(path: Path) -> Path:
    path = path.expanduser().resolve(strict=True)
    if not path.is_file() or not path.stat().st_size:
        raise BundleError(f'Falta un archivo de entrada válido: {path.name}')
    return path


def shader_files(data: Path) -> list[Path]:
    manifest = require_file(data / 'spirv/results.tsv')
    with manifest.open(encoding='utf-8', newline='') as source:
        reader = csv.DictReader(source, delimiter='\t')
        if not {'hash', 'status'}.issubset(reader.fieldnames or ()):
            raise BundleError('results.tsv necesita las columnas hash y status')
        rows = list(reader)
    if not rows:
        raise BundleError('La caché de shaders está vacía')
    seen: set[str] = set()
    files = []
    for row in rows:
        key = row.get('hash', '')
        if not re.fullmatch(r'[0-9a-fA-F]{16,64}', key) or key.lower() in seen:
            raise BundleError('La caché contiene un hash inválido o duplicado')
        seen.add(key.lower())
        if row.get('status') not in ('ok', 'ok-retry'):
            raise BundleError('La caché contiene shaders sin validar')
        path = data / 'spirv' / (key + '.spv')
        if path.is_symlink():
            raise BundleError('Un shader no puede ser un enlace simbólico')
        path = require_file(path)
        with path.open('rb') as stream:
            magic = stream.read(4)
        if magic != b'\x03\x02\x23\x07':
            raise BundleError('Un shader no contiene la cabecera SPIR-V')
        if row.get('spv_bytes') and row['spv_bytes'] != str(path.stat().st_size):
            raise BundleError('El tamaño de un shader no coincide con su manifiesto')
        files.append(path)
    return files


def dependency_paths(text: str) -> list[str]:
    return [line.strip().split(' (', 1)[0] for line in text.splitlines()[1:] if line.strip()]


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def library_input(formula: str, argument: Path | None, execute) -> tuple[Path, Path]:
    location = argument.expanduser() if argument else Path(execute(['brew', '--prefix', formula]).strip())
    filename = LIBRARIES[formula]
    path = require_file(location / 'lib' / filename if location.is_dir() else location)
    prefix = path.parent.parent if path.parent.name == 'lib' else path.parent
    license_name = 'LICENSE.txt' if formula == 'sdl3' else 'LICENSE'
    return path, require_file(prefix / license_name)


def rewrite_libraries(executable: Path, libraries: list[Path], execute) -> None:
    aliases = {'libSDL3.dylib': 'libSDL3.dylib', 'libSDL3.0.dylib': 'libSDL3.dylib',
               'libMoltenVK.dylib': 'libMoltenVK.dylib'}
    for library in libraries:
        execute(['install_name_tool', '-id', '@rpath/' + library.name, str(library)])
    for component in [executable, *libraries]:
        for dependency in dependency_paths(execute(['otool', '-L', str(component)])):
            if dependency.startswith(('/System/', '/usr/lib/')):
                continue
            name = aliases.get(Path(dependency).name)
            if not name:
                raise BundleError('Hay una dependencia externa que no se puede incluir')
            if component in libraries and dependency == '@rpath/' + component.name:
                continue
            replacement = ('@executable_path/../Frameworks/' if component == executable else '@loader_path/') + name
            if dependency != replacement:
                execute(['install_name_tool', '-change', dependency, replacement, str(component)])
        commands = execute(['otool', '-l', str(component)])
        for rpath in dict.fromkeys(re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', commands)):
            execute(['install_name_tool', '-delete_rpath', rpath, str(component)])
    for component in [executable, *libraries]:
        if 'arm64' not in execute(['lipo', '-archs', str(component)]).split():
            raise BundleError('Cada componente debe contener arquitectura arm64')
        allowed = {'@executable_path/../Frameworks/' + p.name for p in libraries} if component == executable else {
            '@loader_path/' + p.name for p in libraries} | {'@rpath/' + component.name}
        if any(not d.startswith(('/System/', '/usr/lib/')) and d not in allowed
               for d in dependency_paths(execute(['otool', '-L', str(component)]))):
            raise BundleError('Queda una dependencia fuera del paquete')


def create_icon(source: Path, resources: Path, temporary: Path, execute) -> None:
    iconset = temporary / 'AppIcon.iconset'
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f'icon_{size}x{size}' + ('@2x' if scale == 2 else '') + '.png'
            execute(['sips', '-s', 'format', 'png', '-z', str(size * scale), str(size * scale),
                     str(source), '--out', str(iconset / name)])
    execute(['iconutil', '-c', 'icns', str(iconset), '-o', str(resources / 'AppIcon.icns')])
    require_file(resources / 'AppIcon.icns')


def rename_exclusive(source: Path, target: Path) -> None:
    # renamex_np(RENAME_EXCL), definido por el SDK de macOS: nunca reemplaza el destino.
    rename = ctypes.CDLL(None, use_errno=True).renamex_np
    rename.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
    rename.restype = ctypes.c_int
    if rename(os.fsencode(source), os.fsencode(target), 0x00000004):
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error))


def install_bundle(source: Path, directory: Path, execute) -> Path:
    directory = absolute(directory)
    no_symlinks(directory)
    if not directory.is_dir():
        raise BundleError('El directorio de instalación debe existir')
    target = directory / source.name
    no_symlinks(target)
    if target.exists():
        raise BundleError('La app de instalación ya existe; no se reemplaza')
    try:
        rename_exclusive(source, target)
    except OSError as error:
        if error.errno != errno.EXDEV:
            raise
        # Entre volúmenes se conserva el original hasta verificar y publicar la única copia nueva.
        with tempfile.TemporaryDirectory(prefix='.castlevania-', dir=directory) as stage:
            candidate = Path(stage) / source.name
            shutil.copytree(source, candidate)
            execute(['codesign', '--verify', '--deep', '--strict', str(candidate)])
            rename_exclusive(candidate, target)
        shutil.rmtree(source)
    return target


def build_bundle(*, binary: Path, data: Path, icon: Path, output: Path,
                 sdl: Path | None = None, moltenvk: Path | None = None,
                 install_dir: Path | None = None, root: Path | None = None, runner=None) -> dict:
    """Construye y verifica; sólo instala si se proporciona install_dir."""
    root = (root or Path(__file__).resolve().parents[1]).resolve()
    target = output_path(root, output)
    if install_dir is not None:
        directory = absolute(install_dir)
        no_symlinks(directory)
        if (not directory.is_dir() or directory / target.name == target
                or (directory / target.name).exists() or (directory / target.name).is_symlink()):
            raise BundleError('La instalación requiere un directorio existente y una app nueva')
    binary, icon = require_file(binary), require_file(icon)
    data = data.expanduser().resolve(strict=True)
    if target.is_relative_to(data) or (install_dir is not None and directory.is_relative_to(data)):
        raise BundleError('El paquete y la instalación deben quedar fuera de los datos de entrada')
    analysis = [require_file(data / 'analysis' / (name + '.bin')) for name in ('main', 'sdk', 'subsdk0')]
    shaders = shader_files(data)
    romfs = (data / 'extract/romfs').resolve(strict=True)
    if not romfs.is_dir() or not any(romfs.iterdir()):
        raise BundleError('RomFS debe ser un directorio no vacío')
    for path in romfs.rglob('*'):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise BundleError('RomFS contiene un enlace o archivo especial')
    commands, records = [], []
    log_path = None
    backend = runner or run

    def execute(arguments):
        commands.append(arguments[0])
        record = {'argumentos': arguments}
        try:
            result = backend(arguments)
            record.update(codigo=0, salida=result[-16384:])
            return result
        except subprocess.CalledProcessError as error:
            record.update(codigo=error.returncode, error=(error.stderr or '')[-16384:])
            raise
        except OSError as error:
            record.update(error=str(error))
            raise
        finally:
            if log_path is None:
                records.append(record)
            else:
                with log_path.open('a', encoding='utf-8') as stream:
                    stream.write(json.dumps(record, ensure_ascii=False) + '\n')

    sdl_file, sdl_license = library_input('sdl3', sdl, execute)
    mvk_file, mvk_license = library_input('molten-vk', moltenvk, execute)
    licenses = {'Runtime-MIT.txt': require_file(root / 'licenses/SuperMarioWonderRecomp-MIT.txt'),
                'Ryubing-MIT.txt': require_file(root / 'licenses/Ryubing-MIT.txt'),
                'SDL3.txt': sdl_license, 'MoltenVK-Apache.txt': mvk_license}
    # Todo se valida antes de copiar RomFS. No se incluyen user/, partidas ni preferencias de la entrada.
    target.parent.mkdir(parents=True, exist_ok=True)
    no_symlinks(target.parent)
    report_path = target.parent / (target.stem + '-paquete-' + uuid.uuid4().hex + '.json')
    log_path = report_path.with_suffix('.jsonl')
    with log_path.open('x', encoding='utf-8') as stream:
        for record in records:
            stream.write(json.dumps(record, ensure_ascii=False) + '\n')
    with tempfile.TemporaryDirectory(prefix='.paquete-', dir=target.parent) as temporary:
        stage = Path(temporary)
        app = stage / target.name
        contents = app / 'Contents'
        resources, frameworks = contents / 'Resources', contents / 'Frameworks'
        executable = contents / 'MacOS/castlevania'
        for directory in (resources, frameworks, executable.parent):
            directory.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(binary, executable)
        executable.chmod(0o755)
        bundled = []
        for name, source in [('libSDL3.dylib', sdl_file), ('libMoltenVK.dylib', mvk_file)]:
            destination = frameworks / name
            shutil.copyfile(source, destination)
            bundled.append(destination)
        rewrite_libraries(executable, bundled, execute)
        build_info = '\n'.join(execute(['vtool', '-show-build', str(p)]) for p in [executable, *bundled])
        minimums = re.findall(r'\bminos\s+(\d+(?:\.\d+){1,2})', build_info)
        if not minimums:
            raise BundleError('No se puede determinar la versión mínima de macOS')
        minimum = max(minimums, key=lambda v: tuple(map(int, v.split('.'))))
        create_icon(icon, resources, stage, execute)
        license_dir = resources / 'Licencias'
        license_dir.mkdir()
        for name, source in licenses.items():
            shutil.copyfile(source, license_dir / name)
        info = {'CFBundleIdentifier': IDENTIFIER, 'CFBundleExecutable': 'castlevania',
                'CFBundleName': 'Castlevania Belmont’s Curse', 'CFBundleDisplayName': 'Castlevania: Belmont’s Curse',
                'CFBundlePackageType': 'APPL', 'CFBundleIconFile': 'AppIcon',
                'CFBundleShortVersionString': VERSION, 'CFBundleVersion': VERSION,
                'LSApplicationCategoryType': 'public.app-category.games', 'LSMinimumSystemVersion': minimum,
                'NSHighResolutionCapable': True, 'NSPrincipalClass': 'NSApplication',
                'SDL_FILESYSTEM_BASE_DIR_TYPE': 'resource'}
        (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
        (contents / 'PkgInfo').write_bytes(b'APPL????')
        game_data = resources / 'GameData'
        (game_data / 'analysis').mkdir(parents=True)
        for source in analysis:
            shutil.copyfile(source, game_data / 'analysis' / source.name)
        shutil.copytree(romfs, game_data / 'extract/romfs')
        (game_data / 'spirv').mkdir()
        for source in shaders:
            shutil.copyfile(source, game_data / 'spirv' / source.name)
        shutil.copyfile(data / 'spirv/results.tsv', game_data / 'spirv/results.tsv')
        (game_data / 'language.txt').write_text('es\n', encoding='utf-8')
        if any(p.is_symlink() for p in app.rglob('*')):
            raise BundleError('El paquete final no puede contener enlaces simbólicos')
        for component in [*bundled, executable, app]:
            execute(['codesign', '--force', '--sign', '-', '--timestamp=none', str(component)])
        execute(['codesign', '--verify', '--deep', '--strict', str(app)])
        report = {'version': VERSION, 'identificador': IDENTIFIER, 'arquitectura': 'arm64',
                  'binario_sha256': sha256(executable), 'icono_original_sha256': sha256(icon),
                  'macos_minimo': minimum, 'shaders': len(shaders), 'firma_local_verificada': True,
                  'instalado': False, 'jugabilidad_verificada': False, 'herramientas': commands,
                  'app': target.name, 'registro': log_path.name,
                  'licencias_sha256': {name: sha256(path) for name, path in licenses.items()}}
        rename_exclusive(app, target)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if install_dir is not None:
        installed = install_bundle(target, install_dir, execute)
        report['instalado'] = True
        report['app'] = installed.name
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, epilog=(
        'Python 3.12+. Requiere las herramientas de Xcode y las licencias de runtime/Ryubing en licenses/. '
        'Las bibliotecas deben conservar LICENSE.txt (SDL3) o LICENSE (MoltenVK) en su prefijo. '
        'Los datos, el icono y el binario se aportan localmente; no se descargan.'))
    required = {'binary': 'Ejecutable Mach-O arm64 compilado localmente',
                'data': 'Carpeta con analysis/*.bin, extract/romfs y spirv/results.tsv',
                'icon': 'Imagen propia del icono NACP; no se incluye un icono predeterminado',
                'output': 'Nueva .app dentro de private/ del repositorio'}
    for option, help_text in required.items():
        parser.add_argument('--' + option, required=True, type=Path, help=help_text)
    for option in ('sdl', 'moltenvk'):
        parser.add_argument('--' + option, type=Path, help='Prefijo o dylib; por defecto se consulta brew --prefix')
    parser.add_argument('--install-dir', type=Path,
                        help='Directorio existente al que mover únicamente la app nueva; no reemplaza otras apps')
    options = parser.parse_args(argv)
    if sys.version_info < (3, 12):
        parser.error('Se necesita Python 3.12 o posterior')
    if sys.platform != 'darwin':
        parser.error('El empaquetado y la firma requieren macOS')
    try:
        report = build_bundle(**vars(options))
    except (BundleError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print('No se creó o instaló el paquete: ' + str(error), file=sys.stderr)
        return 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
