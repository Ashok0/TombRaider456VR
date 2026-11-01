from pathlib import Path
exec(Path('build/inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
 items=value(base+0x66d0e8,'Q')
 for idx in (49,110):
  item=items+idx*9416;print(idx,'status',hex(value(item+0x1820,'I')),'flags',hex(value(item+44,'H')),'meshbits',hex(value(item+12,'I')))
finally:k.CloseHandle(h)
