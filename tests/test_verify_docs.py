import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from scripts.verify_docs import local_target, png_dimensions, verify


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def synthetic_png(extra=b""):
    header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + extra
            + chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00")) + chunk(b"IEND", b""))


class DocumentChecks(unittest.TestCase):
    def make_repository(self, root, include_image=False):
        files = ["PUBLIC_FILES.json"]
        if include_image:
            (root / "sample.png").write_bytes(synthetic_png())
            files.append("sample.png")
        (root / "PUBLIC_FILES.json").write_text(json.dumps(files), encoding="utf-8")

    def test_repository_without_images(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_repository(root)
            self.assertEqual(verify(root), (1, 0, 0))

    def test_image_without_manifest_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_repository(root, include_image=True)
            with self.assertRaisesRegex(ValueError, "Manifiesto"):
                verify(root)

    def test_symlink_in_inventory_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_repository(root)
            (root / "alias.md").symlink_to(root / "PUBLIC_FILES.json")
            with self.assertRaisesRegex(ValueError, "simbólico"):
                verify(root)

    def test_synthetic_png(self):
        self.assertEqual(png_dimensions(synthetic_png()), (1, 1))

    def test_reject_text_metadata(self):
        with self.assertRaises(ValueError):
            png_dimensions(synthetic_png(chunk(b"tEXt", b"Comment\x00dato sintetico")))

    def test_reject_corruption(self):
        data = bytearray(synthetic_png())
        data[20] ^= 1
        with self.assertRaises(ValueError):
            png_dimensions(bytes(data))

    def test_reject_truncation_and_trailing_content(self):
        for data in (synthetic_png()[:-1], synthetic_png() + b"extra"):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                png_dimensions(data)

    def test_link_cannot_escape_root(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            outside = Path(directory) / "outside.md"
            outside.write_text("sintético", encoding="utf-8")
            with self.assertRaises(ValueError):
                local_target(root, root / "README.md", "../outside.md")
            (root / "alias.md").symlink_to(outside)
            with self.assertRaises(ValueError):
                local_target(root, root / "README.md", "alias.md")

    def test_valid_link(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "target.md").write_text("sintético", encoding="utf-8")
            self.assertEqual(local_target(root, root / "README.md", "target.md#seccion"), (root / "target.md").resolve())

    def test_unlisted_file_blocks_export(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "PUBLIC_FILES.json").write_text(json.dumps(["PUBLIC_FILES.json"]), encoding="utf-8")
            (root / "unreviewed.txt").write_text("sintético", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "inventario"):
                verify(root)


if __name__ == "__main__":
    unittest.main()
