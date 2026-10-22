from pathlib import Path
import subprocess

p = Path("src/FirstPerson.cpp")
saved = p.read_bytes()
try:
    needle = b"        IdleTrackedGunHeadingScope idleGunHeading(item);"
    assert saved.count(needle) == 1
    p.write_bytes(saved.replace(needle, b"        // Negative test: original HMD heading reaches native idle gun rotation."))
    with open("build/pistol-idle-turn-negative.log", "w") as log:
        result = subprocess.run(["cmd", "/c", r"build\run_ik_debug_tests.cmd"], stdout=log, stderr=subprocess.STDOUT)
    print("Without idle gun heading override exit:", result.returncode)
    print(Path("build/pistol-idle-turn-negative.log").read_text()[-900:])
    if result.returncode == 0:
        raise SystemExit("Negative control unexpectedly passed")
finally:
    p.write_bytes(saved)
