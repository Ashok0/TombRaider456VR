"""Verify the signed crawl velocities used by first-person steering.
Usage: python tools/verify_crouch_controls.py <game-directory>
Reads installed PDP animation tables; does not execute the native engine.
"""
import struct
import sys
from pathlib import Path


def verify(path):
    data = path.read_bytes()
    count, = struct.unpack_from("<I", data)
    assert 4 + count * 40 <= len(data), (path, "animation table bounds")
    if count < 355:
        return False

    def animation(index):
        offset = 4 + index * 40
        state, = struct.unpack_from("<h", data, offset + 6)
        velocity, acceleration = struct.unpack_from("<ii", data, offset + 8)
        first, last = struct.unpack_from("<hh", data, offset + 24)
        return state, velocity, acceleration, first, last

    assert animation(260)[:3] == (81, 16 << 16, 0), (path, "forward crawl")
    assert animation(276)[:3] == (86, -11 << 16, 0), (path, "negative backward crawl")
    for index in (277, 279):
        state, velocity, acceleration, _, _ = animation(index)
        assert state == 80 and velocity < 0 and acceleration > 0, (path, "negative idle-state stopping clip")
    crouch_states = {71, 80, 81, 84, 85, 86, 105, 106}
    for index in range(count):
        state, velocity, acceleration, first, last = animation(index)
        if state not in crouch_states:
            continue
        assert last >= first, (path, index, "frame range")
        for frame in (first, last):
            speed = (velocity + acceleration * (frame - first)) >> 16
            assert abs(speed) <= 82, (path, index, "native crouch speed exceeds projection guard", speed)
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    for game in ("4", "5"):
        paths = sorted((Path(sys.argv[1]) / game / "DATA").glob("*.PDP"))
        assert paths, (game, "missing PDP files")
        count = sum(verify(path) for path in paths)
        assert count, (game, "missing Lara animations")
        print(f"TR{game}: {count} tables verified: signed crawl velocity, backward stopping clips, bounded native speed")
