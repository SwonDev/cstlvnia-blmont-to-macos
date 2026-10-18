"""La tabla generada conserva el inicializador ELF de un módulo sintético."""

from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def crear_nso(relr=False):
    header = bytearray(0x100)
    header[:4] = b"NSO0"
    segments = [bytearray(0x100) for _ in range(3)]
    for i in range(3):
        struct.pack_into("<III", header, 0x10 + i * 0x10, 0x100 * (i + 1), 0x100 * i, 0x100)
        struct.pack_into("<I", header, 0x60 + 4 * i, 0x100)
    text, ro, data = segments
    struct.pack_into("<II", text, 0, 0x14000004, 0x40)
    struct.pack_into("<I", text, 0x10, 0xD65F03C0)
    struct.pack_into("<I", text, 0x20, 0xD65F03C0)
    text[0x40:0x44] = b"MOD0"
    struct.pack_into("<iiiii", text, 0x44, 0x200 - 0x40, 0x300 - 0x40, 0x300 - 0x40, -0x40, -0x40)
    tags = [(5, 0x118), (6, 0x100), (11, 24), (12, 0x20), (0, 0)]
    if relr:
        struct.pack_into("<i", text, 0x4C, 0x800 - 0x40)
        tags = tags[:-1] + [(36, 0x180), (35, 24), (37, 8), (0, 0)]
        struct.pack_into("<3Q", ro, 0x80, 0x280, 0b1011, 0b11)
        struct.pack_into("<q", data, 0x80, 0x10)
        struct.pack_into("<q", data, 0x88, 0x20)
        struct.pack_into("<q", data, 0x98, -8)
    for i, pair in enumerate(tags):
        struct.pack_into("<qQ", data, i * 16, *pair)
    return bytes(header) + b"".join(segments)


class MetadatosModulo(unittest.TestCase):
    def test_relr_direccion_bitmap_y_bss(self):
        from recomp import nso

        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "relr.nso"
            source.write_bytes(crear_nso(relr=True))
            module = nso.load(str(source))
            self.assertEqual([(r.offset, r.type, r.sym, r.addend) for r in module.relocs],
                             [(0x280, 1027, 0, 0x10), (0x288, 1027, 0, 0x20),
                              (0x298, 1027, 0, -8), (0x480, 1027, 0, 0)])

    def test_relr_rechaza_tabla_truncada(self):
        from recomp import nso

        with tempfile.TemporaryDirectory() as tmp:
            raw = bytearray(crear_nso(relr=True))
            struct.pack_into("<Q", raw, 0x300 + 5 * 16 + 8, 25)
            source = Path(tmp) / "relr-invalido.nso"
            source.write_bytes(raw)
            with self.assertRaisesRegex(ValueError, "RELR"):
                nso.load(str(source))

    def test_relr_rechaza_bitmap_inicial_y_destino_inexistente(self):
        from recomp import nso

        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "relr-invalido.nso"
            for entry in (1, 0x1000):
                raw = bytearray(crear_nso(relr=True))
                struct.pack_into("<Q", raw, 0x280, entry)
                source.write_bytes(raw)
                with self.assertRaisesRegex(ValueError, "RELR"):
                    nso.load(str(source))

    def test_cli_emite_relr_normalizado(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            source = folder / "relr.nso"
            source.write_bytes(crear_nso(relr=True))
            generated = folder / "gen"
            subprocess.run([sys.executable, "-m", "recomp", str(source), str(generated), "--name", "fixture"], cwd=ROOT, check=True, capture_output=True)
            declarations = re.findall(r"void (\w+)\(nx::CPU& c\);", (generated / "funcs.h").read_text())
            consumer = folder / "consumer.cpp"
            consumer.write_text('#include <nx/module.h>\nextern const nx::ModuleInfo nx_module_fixture;\n' +
                                '\n'.join(f'void {name}(nx::CPU&) {{}}' for name in declarations) +
                                '\nint main() { const auto& m = nx_module_fixture; if(m.reloc_count != 4) return 1; '
                                'if(m.relocs[0].offset != 0x280 || m.relocs[0].addend != 0x10) return 2; '
                                'if(m.relocs[2].addend != -8 || m.relocs[3].offset != 0x480 || m.relocs[3].addend != 0) return 3; }\n')
            binary = folder / "consumer"
            subprocess.run(["clang++", "-std=c++20", "-I", str(ROOT / "runtime/include"), str(generated / "table.cpp"), str(consumer), "-o", str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True)

    def test_consumidor_recibe_dt_init_del_nso(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            source = folder / "fixture.nso"
            source.write_bytes(crear_nso())
            generated = folder / "gen"
            result = subprocess.run([sys.executable, "-m", "recomp", str(source), str(generated), "--name", "fixture"], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            declarations = re.findall(r"void (\w+)\(nx::CPU& c\);", (generated / "funcs.h").read_text())
            consumer = folder / "consumer.cpp"
            consumer.write_text('#include <nx/module.h>\nextern const nx::ModuleInfo nx_module_fixture;\n' + '\n'.join(f'void {name}(nx::CPU&) {{}}' for name in declarations) + '\nint main() { const auto& m = nx_module_fixture; if (m.dt_init != 0x20) return 1; for (size_t i=0; i<m.func_count; i++) if (m.funcs[i].offset == m.dt_init) return 0; return 2; }\n')
            binary = folder / "consumer"
            result = subprocess.run(["clang++", "-std=c++20", "-I", str(ROOT / "runtime/include"), str(generated / "table.cpp"), str(consumer), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
