from pathlib import Path
import subprocess
p=Path('src/FirstPersonIK.h'); saved=p.read_bytes()
try:
    s=saved.decode(); assert s.count('if(continuity) {')==1
    p.write_bytes(s.replace('if(continuity) {','if(false && continuity) {').encode())
    with open('build/pistol-glance-negative.log','w') as log:
        result=subprocess.run(['cmd','/c',r'build\run_ik_debug_tests.cmd'],stdout=log,stderr=subprocess.STDOUT)
    print('Without glance continuity exit:',result.returncode)
    print(Path('build/pistol-glance-negative.log').read_text()[-500:])
finally:
    p.write_bytes(saved)
