"""Contratos de preparación con entradas sintéticas; no contienen shaders de juegos."""

from pathlib import Path
import struct
import tempfile
import unittest

from tools.maxwell import unity_shaders


def kernel(raw=False):
    blob = bytearray(0xA80)
    struct.pack_into('<I', blob, 0, 0x98761234)
    start, length = (0, 0x180) if raw else (0x100, 0x80)
    struct.pack_into('<5I', blob, 0x6F8, length, 0, 0x200, 0x200, start)
    struct.pack_into('<3I', blob, 0x748, 4, 4, 4)
    if not raw:
        struct.pack_into('<I', blob, 0x880, 0x12345679)
    struct.pack_into('<Q', blob, 0x880 + start + length - 8, 0xE30000000007000F)
    return blob


class PreparacionShaders(unittest.TestCase):
    def test_compute_crudo_se_admite_solo_con_control_valido(self):
        from scripts.prepare_shaders import scan_embedded
        stages, rejected = scan_embedded(kernel(raw=True), "main")
        self.assertEqual(rejected, [])
        self.assertEqual([(s.kind, s.start, s.code_size) for s in stages], [("compute", 0, 512)])
        invalid = kernel(raw=True)
        struct.pack_into('<3I', invalid, 0x748, 1024, 1024, 1024)
        self.assertEqual(scan_embedded(invalid, "main")[0], [])
        self.assertEqual(scan_embedded(kernel(raw=True)[:-1], "main")[0], [])

    def test_control_incompatible_no_se_deduplica_por_hash_de_codigo(self):
        from scripts.prepare_shaders import ShaderInventory
        with tempfile.TemporaryDirectory() as directory:
            inventory = ShaderInventory(Path(directory))
            blob = kernel()
            stages, _ = unity_shaders.scan_blob(blob, "a")
            inventory.add(blob, stages[0], "a", required=True)
            other = bytearray(blob)
            struct.pack_into('<I', other, 0x748, 8)
            stages, _ = unity_shaders.scan_blob(other, "b")
            with self.assertRaisesRegex(ValueError, "control incompatible"):
                inventory.add(other, stages[0], "b", required=False)

    def test_resultados_invalidos_no_se_promueven_ni_ocultan(self):
        from scripts.prepare_shaders import consolidate_results
        records = {'a': {'required': True}, 'b': {'required': False}}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            rows = [{'hash': 'a', 'status': 'invalid', 'spv_bytes': '4'},
                    {'hash': 'b', 'status': 'unsupported', 'spv_bytes': '0'}]
            report = consolidate_results(records, rows, output)
            self.assertFalse(report['required_valid'])
            self.assertEqual(report['required_failures'], ['a'])
            self.assertEqual(report['additional_failures'], ['b'])
            with self.assertRaisesRegex(ValueError, 'duplicado'):
                consolidate_results(records, rows + [rows[0]], output)
            with self.assertRaisesRegex(ValueError, 'Faltan'):
                consolidate_results(records, rows[:1], output)

    def test_plan_separa_compute3d_y_no_etiqueta_geometry_como_vertex(self):
        from scripts.prepare_shaders import translation_plan
        records = {'8aa7c2a144799b6f': {'stage': 'compute'},
                   'v': {'stage': 'vertex'}, 'g': {'stage': 'geometry'}}
        groups, unsupported = translation_plan(records)
        self.assertEqual(groups, {'2d': ['v'], '3d': ['8aa7c2a144799b6f']})
        self.assertEqual(unsupported, ['g'])

    def test_compute_sin_kernels_se_registra_como_vacio(self):
        from types import SimpleNamespace
        from unittest.mock import patch
        obj = SimpleNamespace(path_id=600, type=SimpleNamespace(name='ComputeShader'),
                              read_typetree=lambda: {'m_Name': 'Vacío', 'variants': [{'kernels': []}]})
        with patch('UnityPy.load', return_value=SimpleNamespace(objects=[obj])):
            self.assertEqual(list(unity_shaders.shader_blobs('fixture')), [('600', 'compute', 'Vacío', bytes(4))])

    def test_ok_sin_archivo_no_puede_publicarse(self):
        from scripts.prepare_shaders import consolidate_results
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, 'SPIR-V ausente'):
                consolidate_results({'a': {'required': True}},
                                    [{'hash': 'a', 'status': 'ok', 'spv_bytes': '4'}], root)

    def test_salida_anterior_se_conserva_sin_ejecutar_herramientas(self):
        from unittest.mock import patch
        from scripts.prepare_shaders import prepare
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'private' / 'castlevania'
            (data / 'spirv').mkdir(parents=True)
            sentinel = data / 'spirv' / 'previo.txt'
            sentinel.write_text('conservar', encoding='utf-8')
            with patch('scripts.prepare_shaders.ROOT', root), patch('subprocess.run') as run:
                with self.assertRaisesRegex(ValueError, 'Ya existe spirv'):
                    prepare(data, 2)
                run.assert_not_called()
            self.assertEqual(sentinel.read_text(encoding='utf-8'), 'conservar')

    def test_manifest_requerido_rechaza_omision_aunque_todos_traduzcan(self):
        import hashlib
        from scripts.prepare_shaders import verify_required_coverage
        expected = hashlib.sha256(b'a\nb\n').hexdigest()
        records = {'a': {'required': True}, 'b': {'required': True}, 'extra': {'required': False}}
        verify_required_coverage(records, expected)
        del records['b']
        with self.assertRaisesRegex(ValueError, 'Cobertura requerida'):
            verify_required_coverage(records, expected)

    def test_build_compartido_excluye_otro_consumidor(self):
        import subprocess
        import sys
        from scripts.prepare_shaders import harness_lock
        with tempfile.TemporaryDirectory() as directory:
            lock = Path(directory) / 'build.lock'
            with harness_lock(lock):
                other = subprocess.run([sys.executable, '-c',
                    'import fcntl,sys; f=open(sys.argv[1], "a"); fcntl.flock(f, fcntl.LOCK_EX|fcntl.LOCK_NB)', str(lock)],
                    capture_output=True)
                self.assertNotEqual(other.returncode, 0)
            with harness_lock(lock):
                pass

    def test_preparacion_rechaza_manifest_incompleto_antes_del_build(self):
        from types import SimpleNamespace
        from unittest.mock import patch
        from scripts.prepare_shaders import prepare
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'private' / 'castlevania'
            (data / 'analysis').mkdir(parents=True)
            for module in ('main', 'sdk', 'subsdk0'):
                (data / 'analysis' / (module + '.bin')).write_bytes('entrada sintética'.encode('utf-8'))
            generator = root / 'private/third_party/ryubing/src/Spv.Generator/Spv.Generator.csproj'
            generator.parent.mkdir(parents=True)
            generator.write_text('fixture', encoding='utf-8')
            with patch('scripts.prepare_shaders.ROOT', root), \
                 patch('scripts.prepare_shaders.extract_inputs', return_value=(SimpleNamespace(records={'a': {'required': True}}), {})), \
                 patch('shutil.which', return_value='/fixture/spirv-val'), patch('subprocess.run') as run:
                with self.assertRaisesRegex(ValueError, 'Cobertura requerida'):
                    prepare(data, 2)
                run.assert_not_called()

    def test_descubre_serializados_sin_extension_y_assets_anidados(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            builtin = root / 'Resources' / 'unity_builtin_extra'
            builtin.parent.mkdir()
            header = bytearray(128)
            struct.pack_into('>4I', header, 0, 0, 0, 22, 0)
            struct.pack_into('>IQQQ', header, 20, 20, 128, 64, 0)
            builtin.write_bytes(header)
            nested = root / 'Resources' / 'a.assets'
            nested.write_bytes(header)
            (root / 'a.resS').write_bytes(bytes(128))
            self.assertEqual(unity_shaders.discover_unity_files(root), [nested, builtin])


if __name__ == '__main__':
    unittest.main()
