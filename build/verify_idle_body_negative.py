from pathlib import Path
import subprocess

cases = [
    ("camera", Path("src/FirstPerson.cpp"),
     b"if(CameraViewFrame(rotation,position) &&", b"if(false && CameraViewFrame(rotation,position) &&"),
    ("angular", Path("src/BodyFollower.h"),
     b"(angle-angularDead)*(1-std::exp(-12*dt))", b"angle*(1-std::exp(-12*dt))"),
]
for name, path, before, after in cases:
    saved = path.read_bytes()
    try:
        assert saved.count(before) == 1
        path.write_bytes(saved.replace(before, after))
        log_path = Path(f"build/idle-body-negative-{name}.log")
        with log_path.open("w") as log:
            result = subprocess.run(["cmd", "/c", r"build\run_ik_debug_tests.cmd"], stdout=log, stderr=subprocess.STDOUT)
        print(name, "without correction exit:", result.returncode)
        print(log_path.read_text()[-500:])
        if result.returncode == 0:
            raise SystemExit("Negative control unexpectedly passed")
    finally:
        path.write_bytes(saved)
