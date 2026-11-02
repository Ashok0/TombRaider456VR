from pathlib import Path
p=Path('C:/dev/TombRaider123VR/src/FirstPerson.cpp')
s=p.read_text(encoding='utf-8-sig')
for a,b in [
    ('void PrepareGroundDirection(uint8_t* item,uint32_t action) {','void PrepareGroundDirection(uint8_t* item,uint32_t action,bool backHop=false) {'),
    ('if (GroundGaitMatchesAction(state,action,BoundGame()) && !stopping && goal==state) return;','if (!backHop && GroundGaitMatchesAction(state,action,BoundGame()) && !stopping && goal==state) return;'),
    ('if (backHopRequested) PrepareGroundDirection(item,Back);','if (backHopRequested) PrepareGroundDirection(item,Back,true);'),
]:
    assert s.count(a)==1,a
    s=s.replace(a,b)
p.write_text(s,encoding='utf-8',newline='\r\n')
t=Path('C:/dev/TombRaider456VR/build/back-hop/back_hop123_test.cpp').read_text()
t=t.replace('C:/dev/TombRaider123VR/tests/locomotion_selftest.cpp','locomotion_selftest.cpp')
Path('C:/dev/TombRaider123VR/tests/back_hop_test.cpp').write_text(t,encoding='utf-8',newline='\r\n')
print('Updated TR1-3 hop handoff and added focused regression fixture.')
