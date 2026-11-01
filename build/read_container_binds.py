from pathlib import Path
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
 for slot in (164,166):
  obj=base+0x697100+slot*3792;n=value(obj,'h');print(slot,n)
  for j in range(n): print(' bind',j,struct.unpack('<16f',read(obj+1676+j*64,64)))
finally:k.CloseHandle(h)
