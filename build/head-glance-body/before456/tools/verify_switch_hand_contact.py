"""Read-only check of the floor handle and visible palm anchors against TRMs."""
from pathlib import Path
import re, math
from verify_revolver_laser_mesh import vertices, centre
root=Path(__file__).resolve().parents[1]
assets=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered\4\ITEM')
source=(root/'src/PhysicalSwitch.inl').read_text()
def anchor(name):
    return tuple(map(float,re.search(r'constexpr motiongun::Vec '+name+r'\{([^}]+)\}',source)[1].split(',')))
grip=anchor('RailHandleLocal');palm=anchor('SwitchPalmMesh');count=0
for path in assets.glob('LEVER_SWITCH*.TRM'):
    mesh=vertices(path)
    tip=[p for p,(j,w) in mesh.items() if j[0]==1 and p[2]<-650]
    c=centre(tip)
    radius=min((max(p[i] for p in tip)-min(p[i] for p in tip))/2 for i in (0,1))
    assert math.hypot(grip[0]-c[0],grip[1]-c[1])<radius,(path,'grip is outside handle tube')
    assert min(p[2] for p in tip)<=grip[2]<-650
    assert math.hypot(c[0],c[1])>radius,'old Action icon offset must fail this check'
    count+=1
for name in ('HAND_BARE_REST.TRM','HAND_GLOVES_REST.TRM','HAND_XRAY_REST.TRM'):
    hand=[p for p,(j,w) in vertices(assets/name).items() if j[0]==10]
    assert all(min(p[i] for p in hand)<palm[i]<max(p[i] for p in hand) for i in range(3)),name
    count+=1
print(f'PASS: physical handle/palm anchors inside {count} installed lever/hand meshes')
