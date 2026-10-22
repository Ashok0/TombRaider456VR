from pathlib import Path
import subprocess
p=Path('src/FirstPerson.cpp')
original=p.read_bytes()
s=original.decode()
try:
    old=s.replace('if(hand==1 && MotionReady() && CurrentMotionWeapon()==1) {','if(false && hand==1 && MotionReady() && CurrentMotionWeapon()==1) {').replace('if(hand==1 && armed && CurrentMotionWeapon()==1 && g_pistolBodyScope) {','if(false && hand==1 && armed && CurrentMotionWeapon()==1 && g_pistolBodyScope) {')
    assert old != s
    p.write_bytes(old.encode())
    with open('build/right-pistol-target-negative.log','w') as log:
        result=subprocess.run(['cmd','/c',r'build\run_ik_debug_tests.cmd'],stdout=log,stderr=subprocess.STDOUT)
    print('Original target regression exit:',result.returncode)
    print(Path('build/right-pistol-target-negative.log').read_text()[-500:])
finally:
    p.write_bytes(original)
try:
    old=s.replace('if(g_renderArm==1 && PistolBodyPoseMatches(item,pass))','if(false && g_renderArm==1 && PistolBodyPoseMatches(item,pass))')
    assert old != s
    p.write_bytes(old.encode())
    with open('build/right-pistol-hand-negative.log','w') as log:
        result=subprocess.run(['cmd','/c',r'build\run_ik_debug_tests.cmd'],stdout=log,stderr=subprocess.STDOUT)
    print('Original hand regression exit:',result.returncode)
    print(Path('build/right-pistol-hand-negative.log').read_text()[-500:])
finally:
    p.write_bytes(original)
