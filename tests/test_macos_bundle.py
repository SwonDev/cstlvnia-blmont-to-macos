"""Fixtures propias: no necesitan ni redistribuyen datos comerciales."""
import errno
import json
import os
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path
import zlib

from scripts import macos_bundle as bundle


class Tools:
    def __init__(self, root):
        self.root = root
        self.calls = []
        self.dependencies = {}
        self.rpaths = {}
        self.arch = 'arm64'
        self.minimum = '15.0'
        self.component_minimums = {}
        self.extra_dependency = None
        self.fail_signature = False

    def __call__(self, args):
        self.calls.append(args)
        tool = args[0]
        if tool == 'brew':
            return str(self.root / args[-1]) + '\n'
        if tool == 'otool':
            path = args[-1]
            if args[1] == '-L':
                deps = self.dependencies.setdefault(path, [
                    '/ruta/original/libSDL3.0.dylib' if Path(path).name == 'castlevania'
                    else '@rpath/' + Path(path).name,
                    '/usr/lib/libSystem.B.dylib'])
                if self.extra_dependency and self.extra_dependency not in deps:
                    deps.append(self.extra_dependency)
                return path + ':\n' + ''.join('\t' + item + ' (compatibility version 1.0.0)\n' for item in deps)
            rpaths = self.rpaths.setdefault(path, ['/ruta/original/lib'])
            return ''.join('cmd LC_RPATH\ncmdsize 40\npath ' + p + ' (offset 12)\n' for p in rpaths)
        if tool == 'install_name_tool':
            path = args[-1]
            if args[1] == '-change':
                self.dependencies[path] = [args[3] if p == args[2] else p for p in self.dependencies[path]]
            elif args[1] == '-delete_rpath':
                self.rpaths[path].remove(args[2])
            return ''
        if tool == 'lipo':
            return self.arch
        if tool == 'vtool':
            minimum = self.component_minimums.get(Path(args[-1]).name, self.minimum)
            return 'platform MACOS\nminos ' + minimum + '\nsdk 27.0\n'
        if tool == 'sips':
            Path(args[-1]).write_bytes(b'PNG propio')
            return ''
        if tool == 'iconutil':
            Path(args[-1]).write_bytes(b'icns propio')
            return ''
        if tool == 'codesign':
            if '--verify' in args and self.fail_signature:
                raise subprocess.CalledProcessError(1, args, stderr='Firma no válida')
            return ''
        raise AssertionError(args)


class Project(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.binary = self.write('build/castlevania', b'binario propio')
        self.icon = self.write('entrada/icono.png', b'icono propio')
        self.data = self.root / 'datos'
        for name in ('main', 'sdk', 'subsdk0'):
            self.write('datos/analysis/' + name + '.bin', b'datos propios')
        self.write('datos/extract/romfs/propio.txt', b'recurso propio')
        self.key = '0123456789abcdef'
        self.spv = self.write('datos/spirv/' + self.key + '.spv', b'\x03\x02\x23\x07' + b'\0' * 16)
        self.manifest = self.write('datos/spirv/results.tsv', b'hash\tstatus\tspv_bytes\n0123456789abcdef\tok\t20\n')
        self.write('datos/user/NO_COPIAR', b'partida propia')
        for name in ('SuperMarioWonderRecomp-MIT.txt', 'Ryubing-MIT.txt', 'FidelityFX-FSR-MIT.txt'):
            self.write('licenses/' + name, b'Licencia MIT de fixture propio\n')
        for formula, name in bundle.LIBRARIES.items():
            self.write(formula + '/lib/' + name, b'dylib propia')
            self.write(formula + ('/LICENSE.txt' if formula == 'sdl3' else '/LICENSE'), b'Licencia propia\n')
        self.output = self.root / 'private/paquetes/Juego.app'
        self.tools = Tools(self.root)

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def build(self, **changes):
        args = dict(binary=self.binary, data=self.data, icon=self.icon, output=self.output,
                    root=self.root, runner=self.tools)
        args.update(changes)
        return bundle.build_bundle(**args)

    def assert_no_package(self):
        self.assertFalse(self.output.exists())
        self.assertFalse(list(self.output.parent.glob('.paquete-*')))


class BundlePaths(Project):
    def test_output_must_be_new_and_private(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            with self.assertRaises(bundle.BundleError):
                bundle.output_path(root, root / 'public.app')
            target = root / 'private' / 'paquetes' / 'Juego.app'
            self.assertEqual(bundle.output_path(root, target), target)
            target.mkdir(parents=True)
            with self.assertRaises(bundle.BundleError):
                bundle.output_path(root, target)

    def test_symlink_destinations_and_ancestors_are_rejected(self):
        real = self.root / 'destino'
        real.mkdir()
        private = self.root / 'private'
        private.symlink_to(real, target_is_directory=True)
        with self.assertRaises(bundle.BundleError):
            self.build()
        self.assertEqual(list(real.iterdir()), [])

    def test_data_cannot_contain_output_or_installation(self):
        private_data = self.root / 'private/datos'
        private_data.parent.mkdir()
        shutil.move(self.data, private_data)
        with self.assertRaises(bundle.BundleError):
            self.build(data=private_data, output=private_data / 'Juego.app')
        with self.assertRaises(bundle.BundleError):
            self.build(data=private_data, install_dir=private_data)
        self.assertEqual(self.tools.calls, [])

    def test_install_destination_must_be_new_and_distinct(self):
        self.output.parent.mkdir(parents=True)
        with self.assertRaises(bundle.BundleError):
            self.build(install_dir=self.output.parent)
        destination = self.root / 'Aplicaciones'
        destination.mkdir()
        (destination / self.output.name).mkdir()
        with self.assertRaises(bundle.BundleError):
            self.build(install_dir=destination)
        self.assertEqual(self.tools.calls, [])

    def test_installation_rejects_symbolic_directory_and_dangling_app(self):
        destination = self.root / 'Aplicaciones'
        destination.mkdir()
        link = self.root / 'Enlace'
        link.symlink_to(destination, target_is_directory=True)
        with self.assertRaises(bundle.BundleError):
            self.build(install_dir=link)
        (destination / self.output.name).symlink_to(self.root / 'ausente')
        with self.assertRaises(bundle.BundleError):
            self.build(install_dir=destination)
        self.assertEqual(self.tools.calls, [])


class Inputs(Project):
    def test_shaders_require_valid_manifest_and_binary(self):
        for text in ('hash\tstatus\n../escape\tok\n',
                     'hash\tstatus\n0123456789abcdef\tfailed\n',
                     'hash\tstatus\n0123456789abcdef\tok\n0123456789abcdef\tok\n',
                     'hash\tstatus\n', 'otra\tcolumna\n1\t2\n',
                     'hash\tstatus\tspv_bytes\n0123456789abcdef\tok\t99\n'):
            with self.subTest(text=text):
                self.manifest.write_text(text, encoding='utf-8')
                with self.assertRaises(bundle.BundleError):
                    self.build()
                self.assertEqual(self.tools.calls, [])
        self.manifest.write_text('hash\tstatus\n0123456789abcdef\tok-retry\n', encoding='utf-8')
        self.assertEqual(bundle.shader_files(self.data), [self.spv])
        self.spv.write_bytes(b'incorrecto')
        with self.assertRaises(bundle.BundleError):
            self.build()
        self.assert_no_package()

    def test_missing_input_empty_romfs_and_symlink_are_rejected(self):
        for file in (self.binary, self.icon, self.data / 'analysis/main.bin', self.spv):
            with self.subTest(file=file):
                original = file.read_bytes()
                file.unlink()
                with self.assertRaises((bundle.BundleError, OSError)):
                    self.build()
                file.write_bytes(original)
        (self.data / 'extract/romfs/propio.txt').unlink()
        with self.assertRaises(bundle.BundleError):
            self.build()
        (self.data / 'extract/romfs/enlace').symlink_to(self.binary)
        with self.assertRaises(bundle.BundleError):
            self.build()
        self.assert_no_package()

    def test_shader_symlink_is_rejected(self):
        self.spv.unlink()
        self.spv.symlink_to(self.binary)
        with self.assertRaises(bundle.BundleError):
            self.build()

    def test_dependency_discovery_accepts_explicit_file_and_prefix(self):
        self.assertEqual(bundle.library_input('sdl3', None, self.tools)[0], self.root / 'sdl3/lib/libSDL3.dylib')
        self.assertEqual(bundle.library_input('molten-vk', self.root / 'molten-vk', self.tools)[0],
                         self.root / 'molten-vk/lib/libMoltenVK.dylib')
        bundle.library_input('sdl3', self.root / 'sdl3/lib/libSDL3.dylib', self.tools)
        self.assertEqual(sum(call[0] == 'brew' for call in self.tools.calls), 1)


@unittest.skipUnless(sys.platform == 'darwin', 'renamex_np requiere macOS')
class Packaging(Project):
    def test_autonomous_package_and_private_metadata(self):
        report = self.build()
        contents = self.output / 'Contents'
        info = plistlib.loads((contents / 'Info.plist').read_bytes())
        self.assertEqual(info['CFBundleIdentifier'], 'dev.swondev.castlevania')
        self.assertEqual(info['CFBundleShortVersionString'], '1.1.4')
        self.assertEqual(info['SDL_FILESYSTEM_BASE_DIR_TYPE'], 'resource')
        self.assertEqual(info['LSMinimumSystemVersion'], '15.0')
        resources = contents / 'Resources'
        self.assertEqual(len(list((resources / 'Licencias').iterdir())), 5)
        self.assertEqual((resources / 'Licencias/FidelityFX-FSR-MIT.txt').read_bytes(),
                         (self.root / 'licenses/FidelityFX-FSR-MIT.txt').read_bytes())
        self.assertEqual((resources / 'GameData/language.txt').read_text(), 'es\n')
        self.assertFalse((resources / 'GameData/user').exists())
        self.assertEqual((resources / 'GameData/extract/romfs/propio.txt').read_bytes(), b'recurso propio')
        self.assertFalse(any(p.is_symlink() for p in self.output.rglob('*')))
        reports = list(self.output.parent.glob('*-paquete-*.json'))
        self.assertEqual(len(reports), 1)
        self.assertEqual(json.loads(reports[0].read_text()), report)
        records = [json.loads(line) for line in (self.output.parent / report['registro']).read_text().splitlines()]
        self.assertEqual(len(records), len(self.tools.calls))
        self.assertEqual(records[-1]['argumentos'][:4], ['codesign', '--verify', '--deep', '--strict'])
        self.assertFalse(report['instalado'])
        self.assertFalse(report['jugabilidad_verificada'])
        self.assertEqual(len([c for c in self.tools.calls if c[0] == 'codesign' and '--sign' in c]), 4)
        self.assertTrue(any(c[:4] == ['codesign', '--verify', '--deep', '--strict'] for c in self.tools.calls))
        self.assertTrue(any(c[0:2] == ['install_name_tool', '-change'] and '@executable_path' in c[3]
                            for c in self.tools.calls))
        self.assertTrue(all(not paths for paths in self.tools.rpaths.values()))

    def test_language_is_preserved_for_each_supported_locale(self):
        for language in ('es', 'en', 'ja'):
            with self.subTest(language=language):
                source = self.write('datos/language.txt', (language + '\n').encode())
                output = self.output.with_name(language + '.app')
                self.build(output=output)
                self.assertEqual((output / 'Contents/Resources/GameData/language.txt').read_bytes(),
                                 source.read_bytes())

    def test_invalid_language_is_rejected_before_packaging(self):
        for index, language in enumerate(('fr', 'ES', '../es', 'es\nja', '')):
            with self.subTest(language=language):
                self.output = self.output.with_name('invalido-' + str(index) + '.app')
                self.write('datos/language.txt', language.encode())
                with self.assertRaises((bundle.BundleError, OSError)):
                    self.build()
                self.assert_no_package()
                self.assertEqual(self.tools.calls, [])

    def test_fidelityfx_license_is_required_before_packaging(self):
        (self.root / 'licenses/FidelityFX-FSR-MIT.txt').unlink()
        with self.assertRaises((bundle.BundleError, OSError)):
            self.build()
        self.assert_no_package()

    def test_minimum_macos_is_maximum_of_all_components(self):
        self.tools.component_minimums = {'castlevania': '15.0', 'libSDL3.dylib': '26.0',
                                       'libMoltenVK.dylib': '12.0'}
        report = self.build()
        info = plistlib.loads((self.output / 'Contents/Info.plist').read_bytes())
        self.assertEqual(info['LSMinimumSystemVersion'], '26.0')
        self.assertEqual(report['macos_minimo'], '26.0')

    def test_signature_failure_external_dependency_and_non_arm64_do_not_publish(self):
        self.tools.fail_signature = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.build()
        self.assert_no_package()
        failure_log = next(self.output.parent.glob('*-paquete-*.jsonl'))
        self.assertEqual(json.loads(failure_log.read_text().splitlines()[-1])['codigo'], 1)
        self.tools = Tools(self.root)
        self.tools.extra_dependency = '/otra/libDesconocida.dylib'
        with self.assertRaises(bundle.BundleError):
            self.build()
        self.assert_no_package()
        self.tools = Tools(self.root)
        self.tools.arch = 'x86_64'
        with self.assertRaises(bundle.BundleError):
            self.build()
        self.assert_no_package()

    def test_missing_deployment_target_does_not_publish(self):
        self.tools.minimum = 'desconocida'
        with self.assertRaises(bundle.BundleError):
            self.build()
        self.assert_no_package()

    def test_publish_race_does_not_overwrite_new_destination(self):
        def racing(args):
            result = self.tools(args)
            if args[0] == 'codesign' and '--verify' in args:
                self.output.mkdir()
                (self.output / 'conservar').write_text('existente')
            return result
        with self.assertRaises(OSError):
            self.build(runner=racing)
        self.assertEqual((self.output / 'conservar').read_text(), 'existente')

    def test_optional_install_moves_only_new_app(self):
        destination = self.root / 'Aplicaciones'
        destination.mkdir()
        self.write('Aplicaciones/Otra.app/conservar', b'existente')
        report = self.build(install_dir=destination)
        self.assertTrue(report['instalado'])
        self.assertFalse(self.output.exists())
        self.assertTrue((destination / self.output.name / 'Contents/Info.plist').is_file())
        self.assertEqual((destination / 'Otra.app/conservar').read_bytes(), b'existente')

    def test_cross_volume_install_verifies_before_removing_original(self):
        self.build()
        destination = self.root / 'Aplicaciones'
        destination.mkdir()
        original = bundle.rename_exclusive
        calls = []
        def cross_volume(source, target):
            calls.append((source, target))
            if len(calls) == 1:
                raise OSError(errno.EXDEV, 'Otro volumen')
            return original(source, target)
        with mock.patch.object(bundle, 'rename_exclusive', side_effect=cross_volume):
            installed = bundle.install_bundle(self.output, destination, self.tools)
        self.assertFalse(self.output.exists())
        self.assertTrue(installed.is_dir())
        self.assertEqual(len(calls), 2)

    def test_cross_volume_failed_verification_preserves_original(self):
        self.build()
        destination = self.root / 'Aplicaciones'
        destination.mkdir()
        self.tools.fail_signature = True
        with mock.patch.object(bundle, 'rename_exclusive', side_effect=OSError(errno.EXDEV, 'Otro volumen')):
            with self.assertRaises(subprocess.CalledProcessError):
                bundle.install_bundle(self.output, destination, self.tools)
        self.assertTrue(self.output.is_dir())
        self.assertEqual(list(destination.iterdir()), [])


class CommandLine(unittest.TestCase):
    def test_help_and_required_icon(self):
        script = Path(bundle.__file__)
        help_result = subprocess.run([sys.executable, str(script), '--help'], capture_output=True, text=True)
        self.assertEqual(help_result.returncode, 0)
        self.assertIn('--install-dir', help_result.stdout)
        missing = subprocess.run([sys.executable, str(script), '--binary', 'propio', '--data', 'propios',
                                  '--output', 'private/Juego.app'], capture_output=True, text=True)
        self.assertEqual(missing.returncode, 2)
        self.assertIn('--icon', missing.stderr)


@unittest.skipUnless(sys.platform == 'darwin' and os.environ.get('CASTLEVANIA_BUNDLE_NATIVE_TEST') == '1',
                     'Prueba nativa opcional: CASTLEVANIA_BUNDLE_NATIVE_TEST=1')
class NativeMac(Project):
    def test_real_tools_signing_and_relocated_execution(self):
        prefix = Path(bundle.run(['brew', '--prefix', 'sdl3']).strip())
        mvk = Path(bundle.run(['brew', '--prefix', 'molten-vk']).strip())
        source = self.write('build/propio.c', b'''#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <stdio.h>
int main(void) {
    if (!SDL_Init(0)) return 1;
    const char *base = SDL_GetBasePath();
    if (!base) return 2;
    char path[4096];
    snprintf(path, sizeof(path), "%sGameData/analysis/main.bin", base);
    FILE *data = fopen(path, "rb");
    if (!data) return 3;
    fclose(data);
    snprintf(path, sizeof(path), "%s../Frameworks/libMoltenVK.dylib", base);
    void *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!library || !dlsym(library, "vkGetInstanceProcAddr")) return 4;
    dlclose(library);
    SDL_Quit();
    return 0;
}
''')
        bundle.run(['clang', '-arch', 'arm64', '-Wl,-headerpad_max_install_names',
                    '-I' + str(prefix / 'include'), '-L' + str(prefix / 'lib'),
                    '-Wl,-rpath,' + str(prefix / 'lib'), str(source), '-lSDL3', '-o', str(self.binary)])
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1024, 1024, 8, 2, 0, 0, 0))
        image += chunk(b'IDAT', zlib.compress((b'\0' + b'\x80\x20\x40' * 1024) * 1024)) + chunk(b'IEND', b'')
        self.icon.write_bytes(image)
        nacp_icon = self.icon.with_name('iconoNACP.dat')
        bundle.run(['sips', '-s', 'format', 'jpeg', str(self.icon), '--out', str(nacp_icon)])
        installed_dir = self.root / 'Aplicaciones propias'
        installed_dir.mkdir()
        report = self.build(runner=bundle.run, icon=nacp_icon, sdl=prefix, moltenvk=mvk, install_dir=installed_dir)
        app = installed_dir / self.output.name
        bundle.run(['codesign', '--verify', '--deep', '--strict', str(app)])
        result = subprocess.run([str(app / 'Contents/MacOS/castlevania')], capture_output=True, text=True,
                                env={k: v for k, v in os.environ.items() if not k.startswith('DYLD_')})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(report['firma_local_verificada'])


if __name__ == '__main__':
    unittest.main()
