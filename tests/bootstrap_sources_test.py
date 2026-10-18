"""La identidad del traductor depende del archivo fijado, no de un marcador editable."""

import hashlib
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from scripts import bootstrap_sources as bootstrap


class TranslatorIdentity(unittest.TestCase):
    def fixture(self, root):
        parent = root / "private/third_party"
        destination = parent / "ryubing"
        destination.mkdir(parents=True)
        archive = parent / "ryubing-a82350bb77.tar.gz"
        files = {"LICENSE.txt": b"licencia sintetica", "Directory.Build.props": b"<Project />",
                 "src/Ryujinx.Common/Common.cs": b"class Common {}",
                 "src/Spv.Generator/Generator.cs": b"class Generator {}",
                 "src/Ryujinx.Graphics.Shader/Shader.cs": b"class Shader {}"}
        with tarfile.open(archive, "w:gz") as output:
            for name, content in files.items():
                member = tarfile.TarInfo("ryubing/" + name)
                member.size = len(content)
                output.addfile(member, io.BytesIO(content))
                path = destination / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
        return destination, archive, hashlib.sha256(archive.read_bytes()).hexdigest()

    def test_pristine_sources_and_build_outputs_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            destination, _, digest = self.fixture(root)
            output = destination / "src/Ryujinx.Common/obj/generated.cs"
            output.parent.mkdir()
            output.write_text("generado", encoding="utf-8")
            with patch.object(bootstrap, "SHA256", digest):
                self.assertEqual(bootstrap.verify_checkout(root), destination)

    def test_modified_source_rejected_despite_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            destination, _, digest = self.fixture(root)
            (destination / ".port-revision").write_text(bootstrap.REVISION)
            (destination / "src/Ryujinx.Common/Common.cs").write_text("alterado")
            with patch.object(bootstrap, "SHA256", digest), self.assertRaisesRegex(ValueError, "modificada"):
                bootstrap.verify_checkout(root)

    def test_added_build_configuration_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            destination, _, digest = self.fixture(root)
            (destination / "Directory.Build.targets").write_text("alterado")
            with patch.object(bootstrap, "SHA256", digest), self.assertRaisesRegex(ValueError, "adicional"):
                bootstrap.verify_checkout(root)

    def test_modified_archive_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, digest = self.fixture(root)
            archive.write_bytes(archive.read_bytes() + b"extra")
            with patch.object(bootstrap, "SHA256", digest), self.assertRaisesRegex(ValueError, "SHA-256"):
                bootstrap.verify_checkout(root)


if __name__ == "__main__":
    unittest.main()
