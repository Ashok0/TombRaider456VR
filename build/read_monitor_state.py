from pathlib import Path
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
 items=value(base+0x66d0e8,'Q');item=items+32*9416
 print('status',hex(value(item+0x1820,'I')),'flags',hex(value(item+44,'H')),'meshes',hex(value(item+12,'I')),'matrix',struct.unpack('<12f',read(item+7832,48)))
finally:k.CloseHandle(h)
