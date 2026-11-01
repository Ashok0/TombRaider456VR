from pathlib import Path
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
 items=value(base+0x66d0e8,'Q');item=items+97*9416
 print('matrix',struct.unpack('<24f',read(item+7832,96)))
finally:k.CloseHandle(h)
