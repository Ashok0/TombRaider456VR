from pathlib import Path
exec(Path(__file__).with_name('inspect_floor_lever.py').read_text().split('try:\n')[0])
try:
 assert value(base+value(base+0x3c,'I')+8,'I')==0x68c12fe9
 lara=value(base+0x4ee840,'Q');pos=struct.unpack('<iii',read(lara+96,12));items=value(base+0x66d0e8,'Q')
 count=value(base+0x65b234,'i');assert 0<count<32768
 result=[]
 for i in range(count):
  item=items+i*9416;xyz=struct.unpack('<iii',read(item+96,12))
  if sum((a-b)**2 for a,b in zip(pos,xyz))>2048**2:continue
  slot=value(item+16,'h')
  if slot<0:continue
  obj=base+0x697100+slot*3792
  result.append(dict(index=i,slot=slot,position=xyz,rotation=struct.unpack('<3h',read(item+108,6)),
   states=struct.unpack('<5h',read(item+18,10)),type=value(item+48,'h'),joints=value(obj,'h'),
   animationBase=value(obj+66,'h'),collision=hex(value(obj+56,'Q')-base),control=hex(value(obj+24,'Q')-base),
   geomName=read(value(obj+112,'Q'),64).split(b'\0')[0].decode(errors='replace') if value(obj+112,'Q') else '',
   bind=struct.unpack('<'+str(value(obj,'h')*16)+'f',read(obj+1676,value(obj,'h')*64)) if 0<value(obj,'h')<5 else []))
 print(json.dumps(dict(lara=pos,nearby=result),indent=2))
 Path(__file__).with_name('valve-live.json').write_text(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
