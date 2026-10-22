from pathlib import Path
import subprocess
p=Path('src/FirstPerson.cpp')
saved=p.read_bytes()
try:
    source=saved.decode()
    assert source.count('!firstperson::PistolArmReference(')==2
    p.write_bytes(source.replace('!firstperson::PistolArmReference(', 'false && !firstperson::PistolArmReference(').encode())
    with open('build/pistol-bind-negative.log','w') as log:
        result=subprocess.run(['cmd','/c',r'build\run_ik_debug_tests.cmd'],stdout=log,stderr=subprocess.STDOUT)
    print('Native-animation reference exit:',result.returncode)
    print(Path('build/pistol-bind-negative.log').read_text()[-700:])
finally:
    p.write_bytes(saved)
