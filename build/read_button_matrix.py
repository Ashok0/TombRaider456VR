from pathlib import Path
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
 items=value(base+0x66d0e8,'Q');obj=base+0x697100+270*3792;item=items+110*9416
 print('matrices',struct.unpack('<24f',read(item+7832,96)))
finally:k.CloseHandle(h)
