"""La preparación pública rechaza revisiones distintas y conserva los datos aportados."""

import hashlib
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest

from scripts.build_macos import generator_fingerprint, validate_input


class InputContract(unittest.TestCase):
    def test_generation_cache_depends_on_module_identity_and_base(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "games/castlevania").mkdir(parents=True)
            (root / "recomp").mkdir()
            for name in ("games/castlevania/resolution-patches.json", "requirements.txt", "recomp/emit.py"):
                (root / name).write_text("sintetico")
            module = {"name": "main", "base": "0x4000000000", "image_sha256": "abc"}
            initial = generator_fingerprint(root, {"modules": [module]})
            module["base"] = "0x4010000000"
            self.assertNotEqual(initial, generator_fingerprint(root, {"modules": [module]}))
            module["base"] = "0x4000000000"
            module["image_sha256"] = "def"
            self.assertNotEqual(initial, generator_fingerprint(root, {"modules": [module]}))

    def fixture(self, directory):
        root = Path(directory)
        exefs, romfs = root / "exefs", root / "romfs"
        exefs.mkdir()
        (romfs / "Data").mkdir(parents=True)
        icon = root / "icon.jpg"
        icon.write_bytes(b"icono sintetico")
        header = bytearray(256)
        header[:4] = b"NSO0"
        header[64:96] = b"\x11" * 32
        (exefs / "main").write_bytes(header)
        specification = {"modules": [{"name": "main", "build_id": "11" * 32, "image_bytes": 3,
            "image_sha256": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"}]}
        return exefs, romfs, icon, specification

    def test_matching_input_is_read_only(self):
        with tempfile.TemporaryDirectory() as directory:
            exefs, romfs, icon, specification = self.fixture(directory)
            original = (exefs / "main").read_bytes()
            config, modules = validate_input(exefs, romfs, icon, specification,
                                             lambda path, name: SimpleNamespace(image=b"abc"))
            self.assertEqual(config["romfs"], str(romfs.resolve()))
            self.assertEqual(len(modules), 1)
            self.assertEqual((exefs / "main").read_bytes(), original)

    def test_wrong_build_id_rejected_before_loader(self):
        with tempfile.TemporaryDirectory() as directory:
            exefs, romfs, icon, specification = self.fixture(directory)
            specification["modules"][0]["build_id"] = "22" * 32
            def forbidden_loader(path, name):
                self.fail("No debe analizar otra revisión")
            with self.assertRaisesRegex(ValueError, "Build ID"):
                validate_input(exefs, romfs, icon, specification, forbidden_loader)

    def test_corrupt_image_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            exefs, romfs, icon, specification = self.fixture(directory)
            with self.assertRaisesRegex(ValueError, "Imagen no compatible"):
                validate_input(exefs, romfs, icon, specification,
                               lambda path, name: SimpleNamespace(image=b"abd"))

    def test_wrong_romfs_level_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            exefs, romfs, icon, specification = self.fixture(directory)
            with self.assertRaisesRegex(ValueError, "RomFS"):
                validate_input(exefs, romfs / "Data", icon, specification,
                               lambda path, name: SimpleNamespace(image=b"abc"))


if __name__ == "__main__":
    unittest.main()
