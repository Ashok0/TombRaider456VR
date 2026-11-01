from pathlib import Path
import time,subprocess
exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
mod=int(sys.argv[3]);duration=float(sys.argv[4]) if len(sys.argv)>4 else 8
try:
 symbols={}
 out=subprocess.check_output([sys.executable,'tools/pdbdump.py','build/vr-options-release/TombRaiderVR.dll'],text=True,stderr=subprocess.DEVNULL)
 for line in out.splitlines():
  x=line.split()
  if len(x)==4 and x[1]=='data':symbols[x[3].split('::')[-1]]=int(x[0],16)
 wanted={'g_active':'B','g_haveHeading':'B','g_scenePoseValid':'B','g_crouchDrive':'B','g_physicalCrouchCamera':'B','g_heightCrouch':'B','g_runtimeEnabled':'B','g_headingBase':'f','g_headingItem':'Q','g_haveManualInput':'B','g_manualLocal':'2f'}
 rows=[];end=time.time()+duration
 while time.time()<end:
  lara=value(base+0x4ee840,'Q')
  row={n:(struct.unpack('<'+fmt,read(mod+symbols[n],struct.calcsize('<'+fmt))) if fmt.startswith('2') else value(mod+symbols[n],fmt)) for n,fmt in wanted.items() if n in symbols}
  row.update(state=struct.unpack('<5h',read(lara+18,10)),position=struct.unpack('<iii3h',read(lara+96,18)),room=value(lara+28,'h'),camera=value(base+0x66d020+32,'i'),gun=struct.unpack('<3h',read(base+0x4ee680+2,6)),flags=hex(value(base+0x4ee680+68,'I')),script=[value(base+a,'i') for a in (0x1b6b24,0x1b6b1c,0x1b6b54,0x1b6b00,0x1b6b20)],analog=struct.unpack('<4h',read(base+0x65aae0,8)),input=hex(value(base+0x65ab80,'Q')))
  if not rows or row!=rows[-1]:rows.append(row)
  time.sleep(.1)
 Path('build/crouch-live.json').write_text(json.dumps(rows,indent=2))
 print(json.dumps(rows[-12:],indent=2))
finally:k.CloseHandle(h)
