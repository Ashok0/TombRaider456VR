"""Verify TR4 laser aperture/axis against installed TRM assets (read only).

Usage: python tools/verify_revolver_laser_mesh.py <game-directory>
"""
import struct
import sys
from pathlib import Path


def vertices(path):
    data = path.read_bytes()
    magic, materials = struct.unpack_from('<II', data)
    assert magic == 0x024D5254
    offset = 8 + materials * 44
    tiles, = struct.unpack_from('<I', data, offset)
    offset += 4 + ((tiles + 1) // 2) * 4
    bones, indices, count = struct.unpack_from('<III', data, offset)
    assert bones == 0, 'expected shared Lara skeleton'
    offset += 12 + ((indices + 1) // 2) * 4
    assert offset + count * 24 == len(data)
    result = {}
    for i in range(count):
        start = offset + i * 24
        point = struct.unpack_from('<3f', data, start)
        result[point] = (data[start + 16:start + 20], data[start + 20:start + 24])
    return result


def centre(points):
    return tuple((min(p[i] for p in points) + max(p[i] for p in points)) / 2 for i in range(3))


if __name__ == '__main__':
    root = Path(sys.argv[1]) / '4' / 'ITEM'
    for variant in ('BARE', 'GLOVES', 'XRAY'):
        base = vertices(root / f'HAND_{variant}_REVOLVER.TRM')
        laser = vertices(root / f'HAND_{variant}_REVOLVER_LASER.TRM')
        added = {p: skin for p, skin in laser.items() if p not in base}
        front = [p for p in added if 82.5 < p[1] < 83]
        rear = [p for p in added if 40 < p[1] < 44.5]
        assert len(front) >= 32 and len(rear) >= 32
        for p in front + rear:
            joints, weights = added[p]
            # Only xyz are skin indices/weights; packed w carries other data.
            assert joints[0] == 10 and weights[:3] == bytes((255, 0, 0))
        a, b = centre(front), centre(rear)
        for measured, expected in zip(a, (56.523163, 82.748322, 59.557335)):
            assert abs(measured - expected) < .0001, (variant, a)
        for measured, expected in zip(b, (56.523163, 42.401154, 50.810116)):
            assert abs(measured - expected) < .0001, (variant, b)
        axis = tuple(a[i] - b[i] for i in range(3))
        # Intermediate tube rings also lie on this axis. Do not mistake either
        # slanted/bevelled end-cap plane's normal for the barrel direction.
        for y, z in ((47.824115, 51.985809), (75.995201, 58.093273)):
            assert any(abs(p[1]-y) < .0001 and abs(p[2]-z) < .0001 for p in added)
            assert abs((y-b[1])*axis[2] - (z-b[2])*axis[1]) < .001
        print(f'OK: {variant}: aperture and tube axis match mesh; full wrist weighting')
