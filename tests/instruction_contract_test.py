"""Compara instrucciones sintéticas recompiladas con la ejecución AArch64 de Unicorn."""

from pathlib import Path
import random
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import capstone
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn import arm64_const as U

from recomp import analysis, emit, nso, ehframe
from tests import difftest as diff

ASM = [
    "sri v0.16b, v1.16b, #8",
    "sri v0.16b, v0.16b, #1",
    "sri v2.4s, v3.4s, #19",
    "sri d2, d3, #64",
    "sli v0.8h, v1.8h, #0",
    "sli v0.8h, v0.8h, #15",
    "sli v2.2d, v3.2d, #37",
    "sli d2, d3, #1",
    'addhn v0.8b, v1.8h, v2.8h',
    'addhn2 v1.16b, v1.8h, v2.8h',
    'addhn v3.4h, v3.4s, v4.4s',
    'addhn2 v4.4s, v3.2d, v4.2d',
    'raddhn v0.8b, v1.8h, v2.8h',
    'raddhn2 v1.8h, v1.4s, v2.4s',
    'raddhn v3.2s, v3.2d, v4.2d',
    'raddhn2 v4.4s, v3.2d, v4.2d',
    'bfc x0, #7, #53',
    'bfc w1, #0, #32',
    'umnegl x0, w1, w2',
    'umnegl x1, w1, w1',
    'csdb',
    'saddlv h0, v1.16b',
    'saddlv s2, v2.4h',
    'saddlv d3, v4.4s',
    'sqabs v0.16b, v1.16b',
    'sqabs v1.4h, v1.4h',
    'sqabs v2.4s, v3.4s',
    'sqabs v2.2d, v3.2d',
    'sqabs b0, b1',
    'sqabs d0, d1',
    'sqshl v0.16b, v1.16b, v2.16b',
    'sqshl v0.4h, v1.4h, v2.4h',
    'sqshl v0.4s, v1.4s, v2.4s',
    'sqshl v0.2d, v1.2d, v2.2d',
    'sqshl b0, b1, b2',
    'sqshl h0, h1, h2',
    'sqshl s0, s1, s2',
    'sqshl d0, d1, d2',
    'sqrshl v0.16b, v1.16b, v2.16b',
    'sqrshl v0.4h, v1.4h, v2.4h',
    'sqrshl v0.4s, v1.4s, v2.4s',
    'sqrshl v0.2d, v1.2d, v2.2d',
    'sqrshl b0, b1, b2',
    'sqrshl h0, h1, h2',
    'sqrshl s0, s1, s2',
    'sqrshl d0, d1, d2',
    'sqshl v0.16b, v1.16b, #7',
    'sqshl v0.4h, v1.4h, #0',
    'sqshl v0.4s, v1.4s, #17',
    'sqshl v0.2d, v1.2d, #63',
    'sqshl d0, d1, #63',
    'sqshl v1.16b, v1.16b, v2.16b',
    'sqrshl v2.2d, v1.2d, v2.2d',
    'rbit v0.16b, v1.16b',
    'rbit v1.16b, v1.16b',
    'rbit v2.8b, v3.8b',
    'rbit w0, w1',
    'rbit x1, x1',
]


def ensamblar(folder, instructions):
    source = folder / "fixture.s"
    source.write_text(".text\n" + "\n".join(instructions) + "\n")
    output = folder / "fixture.o"
    subprocess.run(["clang", "-target", "aarch64-none-elf", "-c", str(source), "-o", str(output)], check=True, capture_output=True)
    raw = output.read_bytes()
    if raw[:6] != b"\x7fELF\x02\x01":
        raise ValueError("El ensamblador debe producir ELF64 little-endian")
    offset = struct.unpack_from("<Q", raw, 0x28)[0]
    size, count, names_index = struct.unpack_from("<HHH", raw, 0x3A)
    sections = [struct.unpack_from("<IIQQQQIIQQ", raw, offset + i * size) for i in range(count)]
    names = sections[names_index]
    strings = raw[names[4]:names[4] + names[5]]
    for section in sections:
        name = strings[section[0]:].split(b"\0", 1)[0]
        if name == b".text":
            return raw[section[4]:section[4] + section[5]]
    raise ValueError("No hay sección .text")


class ContratoInstrucciones(unittest.TestCase):
    def test_fde_vacio_no_elimina_funcion_llamada(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            code = ensamblar(folder, ["bl destino", "ret", "destino:", "mov x0, #42", "ret"])
            module = nso.Module("sintetico", bytearray(code), (0, len(code)), (len(code), 0), (len(code), 0), (len(code), 0))
            with patch("recomp.analysis.ehframe.parse", return_value=[ehframe.Fde(0, 8, 0, 0), ehframe.Fde(8, 0, 0, 0)]):
                model = analysis.analyze(module, log=lambda _: None)
            function = next(f for f in model.functions if f.start == 8)
            self.assertGreater(function.end, function.start)
            emitter = emit.Emitter(model)
            source = folder / "fde.cpp"
            source.write_text('#include <nx/cpu.h>\n#define NXB NX_BASE\n' + emitter.function(function) +
                              f'\nint main() {{ nx::CPU c{{}}; {emitter.fn(8)}(c); return c.x[0] == 42 ? 0 : 1; }}\n')
            executable = folder / "fde"
            include = Path(__file__).resolve().parents[1] / "runtime/include"
            subprocess.run(["clang++", "-std=c++20", f"-I{include}", str(source), "-o", str(executable)], check=True, capture_output=True)
            subprocess.run([str(executable)], check=True)

    def test_tabla_saltos_con_destino_fuera_de_funcion(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            code = ensamblar(folder, ["ret", "br x0", "mov x1, #99", "ret"])
            module = nso.Module("sintetico", bytearray(code), (0, len(code)), (len(code), 0), (len(code), 0), (len(code), 0))
            function = analysis.Function(4, 16)
            model = analysis.Analysis(module, {}, functions=[analysis.Function(0, 4), function], jump_tables={4: [0, 8, 0x20]})
            emitter = emit.Emitter(model)
            body = emitter.function(function)
            source = folder / "saltos.cpp"
            source.write_text('#include <nx/cpu.h>\n#define NXB NX_BASE\n'
                              'namespace nx { void call(CPU& c, uint64_t) { c.x[1] = 77; } }\n'
                              f'void {emitter.fn(0)}(nx::CPU& c) {{ c.x[1] = 42; }}\n' + body +
                              f'\nint main() {{ nx::CPU c{{}}; '
                              f'c.x[0] = NXB; {emitter.fn(4)}(c); if(c.x[1] != 42) return 1; '
                              f'c.x[0] = NXB + 8; {emitter.fn(4)}(c); if(c.x[1] != 99) return 2; '
                              f'c.x[0] = NXB + 0x20; {emitter.fn(4)}(c); if(c.x[1] != 77) return 3; }}\n')
            executable = folder / "saltos"
            include = Path(__file__).resolve().parents[1] / "runtime/include"
            result = subprocess.run(["clang++", "-std=c++20", f"-I{include}", str(source), "-o", str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(executable)], check=True)

    def test_resultados_coinciden_con_aarch64(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            code = ensamblar(folder, ASM)
            module = nso.Module("sintetico", bytearray(code), (0, len(code)), (len(code), 0), (len(code), 0), (len(code), 0))
            model = analysis.Analysis(module, {})
            emitter = emit.Emitter(model)
            decoder = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
            decoder.detail = True
            instructions = list(decoder.disasm(code, 0))
            self.assertEqual(len(instructions), len(ASM))
            statements = [emitter.insn(diff.Fake(i.address), i, {}) for i in instructions]
            executable = diff.build_runner(str(folder), statements)
            rng = random.Random(417)
            states = [(index, i, diff.make_state(rng, i, index)) for index, i in enumerate(instructions) for _ in range(128)]
            for index, instruction in enumerate(instructions):
                if instruction.mnemonic not in {"sqabs", "sqshl", "sqrshl"}:
                    continue
                bits, _ = emitter.vinfo(instruction.operands[0])
                values = [-(1 << (bits - 1)), -(1 << (bits - 1)) + 1, -2, -1, 0, 1, 2, (1 << (bits - 1)) - 1]
                for value in values:
                    for shift in [-128, -bits - 1, -bits, 1 - bits, -1, 0, 1, bits - 1, bits, 127]:
                        fields = list(diff.STATE.unpack(diff.make_state(rng, instruction, index)))
                        lane = (value & ((1 << bits) - 1)).to_bytes(bits // 8, "little")
                        vectors = bytearray(lane * (512 // len(lane)))
                        if instruction.mnemonic != "sqabs" and instruction.operands[2].type == capstone.arm64_const.ARM64_OP_REG:
                            number = emitter.regop(instruction, instruction.operands[2]).n
                            shift_lane = (shift & ((1 << bits) - 1)).to_bytes(bits // 8, "little")
                            vectors[number * 16:(number + 1) * 16] = shift_lane * (16 // len(shift_lane))
                        fields[34], fields[36] = bytes(vectors), 0
                        states.append((index, instruction, diff.STATE.pack(*fields)))
            input_file, output_file = folder / "input.bin", folder / "output.bin"
            input_file.write_bytes(b"".join(state for _, _, state in states))
            subprocess.run([executable, str(input_file), str(output_file)], check=True, capture_output=True)
            outputs = output_file.read_bytes()
            self.assertEqual(len(outputs), len(states) * diff.STATE.size)
            machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
            machine.ctl_set_cpu_model(U.UC_CPU_ARM64_MAX)
            machine.mem_map(diff.NX_BASE, 0x1000)
            machine.mem_write(diff.NX_BASE, code)
            machine.mem_map(diff.BUF, 0x10000)
            for number, (_, instruction, state) in enumerate(states):
                host = diff.STATE.unpack_from(outputs, number * diff.STATE.size)
                registers, vectors, flags, _, crashed = diff.uc_run(machine, module, instruction.address, state, b"")
                label = f"{instruction.mnemonic} {instruction.op_str}, estado {number}"
                self.assertFalse(crashed, label)
                self.assertEqual(host[1], 0, label)
                self.assertEqual(list(host[2:34]), registers, label)
                self.assertEqual(host[34], vectors, label)
                self.assertEqual(host[35], flags, label)
                self.assertEqual(host[36], machine.reg_read(U.UC_ARM64_REG_FPSR), label)
            print(f"{len(instructions)} instrucciones sintéticas, {len(states)} estados: PASS")


if __name__ == "__main__":
    unittest.main()
