import hashlib
import json
import struct
import tempfile
import subprocess
from recomp import analysis, emit
import unittest
from pathlib import Path
from recomp import nso, patches

class InstructionPatches(unittest.TestCase):
    def fixture(self):
        image=bytearray(struct.pack('<II',0x1a88b2a8,0x1a98b288))
        module=nso.Module('main',image,(0,8),(8,0),(8,0),(8,0))
        profile={'module':'main','sha256':hashlib.sha256(image).hexdigest(),'instructions':[
            {'offset':'0','expected':'0x1a88b2a8','replacement':'0x2a1503e8'},
            {'offset':'4','expected':'0x1a98b288','replacement':'0x2a1403e8'}]}
        return module,profile
    def apply(self,module,profile):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'patch.json';path.write_text(json.dumps(profile));patches.apply(module,path)
    def test_hook_compilado_antes_del_prologo(self):
        m=nso.Module('fixture',bytearray(struct.pack('<I',0xd65f03c0)),(0,4),(4,0),(4,0),(4,0))
        m.entry_hooks={0:'retina_fixture'}
        f=analysis.Function(0,4);an=analysis.Analysis(m,{},functions=[f])
        code=emit.Emitter(an).function(f)
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'fixture.cpp';binary=Path(folder)/'fixture'
            source.write_text('#include <nx/cpu.h>\n#define NXB 0ull\n'+code+'\nvoid retina_fixture(nx::CPU& c){c.x[0]=3024;c.x[1]=1701;}\nint main(){nx::CPU c{};sub_00000000(c);return c.x[0]==3024 && c.x[1]==1701?0:1;}')
            subprocess.run(['clang++','-std=c++20','-I',str(Path(__file__).resolve().parents[1]/'runtime/include'),str(source),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True,timeout=3)

    def test_wrapper_conserva_y_llama_original(self):
        m=nso.Module('fixture',bytearray(struct.pack('<II',0x528000e0,0xd65f03c0)),(0,8),(8,0),(8,0),(8,0))
        m.function_wrappers={0:'opciones_fixture'}
        f=analysis.Function(0,8);an=analysis.Analysis(m,{},functions=[f])
        code=emit.Emitter(an).function(f)
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'fixture.cpp';binary=Path(folder)/'fixture'
            source.write_text('#include <nx/cpu.h>\n#define NXB 0ull\n'+code+'\nvoid opciones_fixture(nx::CPU& c,void (*original)(nx::CPU&)){original(c);c.x[0]+=3;}\nint main(){nx::CPU c{};sub_00000000(c);return c.x[0]==10?0:1;}')
            subprocess.run(['clang++','-std=c++20','-I',str(Path(__file__).resolve().parents[1]/'runtime/include'),str(source),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True,timeout=3)

    def test_wrappers_incorrectos_no_modifican_imagen(self):
        bad=[[{'offset':'2','symbol':'valid'}], [{'offset':'8','symbol':'valid'}],
             [{'offset':'0','symbol':'bad;name'}], [{'offset':'0','symbol':'valid'}]*2]
        for value in bad:
            m,p=self.fixture();original=bytes(m.image);p['wrappers']=value
            with self.assertRaises(ValueError):self.apply(m,p)
            self.assertEqual(bytes(m.image),original)
        m,p=self.fixture();p['wrappers']=[{'offset':'0','symbol':'valid'}]
        self.apply(m,p);self.assertEqual(m.function_wrappers,{0:'valid'})

    def test_valid(self):
        m,p=self.fixture();self.apply(m,p)
        self.assertEqual((m.u32(0),m.u32(4)),(0x2a1503e8,0x2a1403e8))
        self.assertEqual(len(m.instruction_patches),2)
    def test_mismatch_atomic(self):
        m,p=self.fixture();original=bytes(m.image);p['instructions'][1]['expected']='0'
        with self.assertRaises(ValueError):self.apply(m,p)
        self.assertEqual(m.image,original)
    def test_identity_and_bounds(self):
        for key,value in [('sha256','bad'),('module','sdk')]:
            m,p=self.fixture();p[key]=value
            with self.assertRaises(ValueError):self.apply(m,p)
        for offset in ['2','8','-4','0']:
            m,p=self.fixture();p['instructions'][1]['offset']=offset
            with self.assertRaises(ValueError):self.apply(m,p)
if __name__=='__main__':unittest.main()
