import sys,difflib
sys.path.insert(0,'tools')
from port_addresses import Image
a=Image('PDB/tomb5.dll');b=Image(r'C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered/5/tomb5.dll')
for im,start in ((a,0x75230),(b,0x76910)):
 print(hex(start),'size',im.ends[im.starts.index(start)]-start)
 for x in im.disasm(start):print(x)
