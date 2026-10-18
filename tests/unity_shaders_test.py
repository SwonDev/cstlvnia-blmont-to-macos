"""Prueba el formato de extracción con un bloque sintético, sin shaders comerciales."""

import struct
import unittest

from tools.maxwell.unity_shaders import scan_blob


class ContratoUnityNVN(unittest.TestCase):
    def test_kernel_compute(self):
        blob = bytearray(0xA80)
        struct.pack_into("<I", blob, 0, 0x98761234)
        struct.pack_into("<5I", blob, 0x6F8, 0x80, 0, 0x200, 0x200, 0x100)
        struct.pack_into("<I", blob, 0x880, 0x12345679)
        stages, errors = scan_blob(blob, "kernel sintético")
        self.assertEqual(errors, [])
        self.assertEqual(len(stages), 1)
        self.assertEqual(stages[0].kind, "compute")

    def test_validacion_y_limites(self):
        blob = bytearray(0x980)
        struct.pack_into("<I", blob, 0, 0x98761234)
        struct.pack_into("<5I", blob, 0x6F8, 0x50, 0, 0x100, 0x100, 0x30)
        struct.pack_into("<I", blob, 0x880, 0x12345678)
        struct.pack_into("<I", blob, 0x8B0, 1 << 10)
        stages, errors = scan_blob(blob, "sintético")
        self.assertEqual(errors, [])
        self.assertEqual(len(stages), 1)
        self.assertEqual((stages[0].kind, stages[0].code_size), ("vertex", 0x100))
        stages, errors = scan_blob(blob[:-1], "sintético")
        self.assertEqual(stages, [])
        self.assertEqual(len(errors), 1)
        blob[0x90] = 123
        struct.pack_into("<I", blob, 0x704, 0xFFFFFFFF)
        self.assertEqual(scan_blob(blob, "sintético")[0], [])


if __name__ == "__main__":
    unittest.main()
