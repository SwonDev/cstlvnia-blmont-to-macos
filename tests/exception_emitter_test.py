"""Los destinos LSDA deben ser accesibles al desenrollar una llamada recompilada."""
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from recomp import analysis, ehframe, emit, nso

ROOT = Path(__file__).resolve().parents[1]

class ExcepcionesEmisor(unittest.TestCase):
    def test_lsda_rechaza_destinos_fuera_de_funcion_y_tablas_truncadas(self):
        m = nso.Module('fixture', bytearray(128), (0,20),(64,64),(128,0),(128,0))
        m.image[64:72] = bytes([0xff,0xff,1,4,0,4,40,0])
        with self.assertRaisesRegex(ValueError, 'fuera'):
            ehframe.landing_pads(m, ehframe.Fde(0,20,64,100))
        m.image[64:69] = bytes([0xff,0xff,1,127,0])
        with self.assertRaisesRegex(ValueError, 'truncada'):
            ehframe.landing_pads(m, ehframe.Fde(0,20,64,100))

    def test_captura_reanuda_landing_pad_sin_repetir_prologo(self):
        image = bytearray(128)
        struct.pack_into('<5I', image, 0, 0x94000010, 0x52800020, 0xD65F03C0, 0x52800540, 0xD65F03C0)
        image[64:72] = bytes([0xff, 0xff, 1, 4, 0, 4, 12, 0])
        m = nso.Module('fixture', image, (0, 20), (64, 64), (128, 0), (128, 0))
        f = analysis.Function(0, 20)
        an = analysis.Analysis(m, {}, functions=[f])
        with patch('recomp.ehframe.parse', return_value=[ehframe.Fde(0, 20, 64, 100)]):
            code = emit.Emitter(an, exceptions=True).function(f)
        self.ejecutar(code)

    def test_longjmp_desde_landing_pad_no_reentra_en_el_manejador(self):
        image = bytearray(160)
        struct.pack_into('<9I', image, 0, 0x94000010, 0x350000c0, 0x94000012,
                         0x52800020, 0xd65f03c0, 0x94000013, 0xd65f03c0, 0x52800540, 0xd65f03c0)
        image[128:136] = bytes([255, 255, 1, 4, 8, 4, 20, 0])
        m = nso.Module('fixture', image, (0, 36), (128, 32), (160, 0), (160, 0))
        f = analysis.Function(0, 36)
        an = analysis.Analysis(m, {}, functions=[f])
        an.plt[64] = 'setjmp'
        with patch('recomp.ehframe.parse', return_value=[ehframe.Fde(0, 36, 128, 140)]):
            code = emit.Emitter(an, exceptions=True).function(f)
        self.ejecutar(code, landing=20, initial='c.x[0]=123;', calls='''
if(t==80) throw GuestException{1,nullptr};
if(t==96) throw LongJmp{123,5};
std::abort();''')

    def ejecutar(self, code, landing=12, initial='', calls='throw GuestException{1,nullptr};'):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'fixture.cpp'
            source.write_text('''#include <nx/exception.h>
#define NXB 0ull
namespace nx {
[[noreturn]] void unimpl(CPU&,uint64_t,const char*) { std::abort(); }
ExceptionFrame::ExceptionFrame(uint64_t, uint64_t, uint64_t, bool) {}
ExceptionFrame::~ExceptionFrame() {}
void ExceptionFrame::before_call(const CPU& c,uint64_t) { saved.save(c); }
uint64_t ExceptionFrame::enter(CPU& c,const GuestException&) { saved.restore(c);return LANDING; }
void call(CPU&,uint64_t t) { CALLS }
}
'''.replace('LANDING', str(landing)).replace('CALLS', calls) + code +
                '\nint main(){nx::CPU c{}; ' + initial + 'sub_00000000(c); return c.x[0]==42 ? 0 : 1;}')
            binary = Path(temporary) / 'fixture'
            result = subprocess.run(['clang++','-std=c++20','-fsanitize=undefined','-I',str(ROOT/'runtime/include'),str(source),'-o',str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(binary)],check=True,timeout=3)

if __name__ == '__main__': unittest.main()
