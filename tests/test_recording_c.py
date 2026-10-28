"""Compile and execute actual firmware C, with hardware/FatFS mocked explicitly."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest
import sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import osc_recording

class RecordingC(unittest.TestCase):
    def test_actual_c(self):
        gcc=shutil.which('gcc') or r'C:\Strawberry\c\bin\gcc.exe'
        if not pathlib.Path(gcc).exists():self.fail('Host C compiler required for recording acceptance tests')
        with tempfile.TemporaryDirectory() as td:
            td=pathlib.Path(td)
            for name in ('record_format','recording'):
                exe=td/(name+'.exe')
                cmd=[gcc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-I'+str(ROOT/'tests/recording_stubs'),'-I'+str(ROOT/'Services/Inc'),
                    str(ROOT/f'tests/{name}_harness.c'),str(ROOT/'Services/Src/record_format.c'),'-o',str(exe)]
                subprocess.run(cmd,check=True,capture_output=True,text=True)
                fixture=td/'R00000001_00000000.OSC'
                result=subprocess.run([str(exe),str(fixture)],check=True,capture_output=True,text=True)
                print(result.stdout.strip())
                if name=='recording':
                    report=osc_recording.validate([fixture])
                    self.assertTrue(report['completed'],report)
                    self.assertGreater(report['known_dropped'],0)
                    self.assertEqual(report['unknown_gaps'],1)
                    self.assertEqual(report['pauses'],1)
                    data=fixture.read_bytes()
                    for start in range(512,len(data),3584):
                        record=osc_recording.record_v2(data[start:start+3584])
                        if record['type']=='DATA':
                            import struct
                            for i,values in enumerate(struct.iter_unpack('<HHH',record['payload'])):
                                self.assertEqual(values,tuple((record['first']+512+i+c*1000)&4095 for c in range(3)))
                    for size in range(512,len(data),3584):
                        fixture.write_bytes(data[:size])
                        self.assertFalse(osc_recording.validate([fixture])['completed'])
                    for offset in (0,100,508,512,600,512+3583,len(data)-20):
                        damaged=bytearray(data);damaged[offset]^=1;fixture.write_bytes(damaged)
                        try:self.assertFalse(osc_recording.validate([fixture])['completed'])
                        except ValueError:pass

if __name__=='__main__':unittest.main()
