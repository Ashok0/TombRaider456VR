"""Read-only verification of the shipped HK rear aperture and skin binding.

Usage: python tools/verify_hk_scope_mesh.py <game-directory>
TRM v2 layout is from LoadTRM / ogl_meshCreate; vertex stride is 24.
No game assets are changed or distributed by this test.
"""
import struct
import sys
from pathlib import Path

def verify(path):
    data = path.read_bytes()
    magic, materials = struct.unpack_from("<II", data)
    assert magic == 0x024D5254
    cursor = 8 + materials * 44
    tiles, = struct.unpack_from("<I", data, cursor)
    cursor += 4 + ((tiles + 1) // 2) * 4
    bones, = struct.unpack_from("<I", data, cursor)
    assert bones == 0, "expected shared Lara skeleton, not an independent skeleton"
    indices, vertices = struct.unpack_from("<II", data, cursor + 4)
    cursor += 12 + ((indices + 1) // 2) * 4
    assert cursor + vertices * 24 == len(data)
    aperture = []
    for i in range(vertices):
        offset = cursor + i * 24
        x, y, z = struct.unpack_from("<3f", data, offset)
        if 54 < x < 73 and 38 < y < 44 and 69 < z < 89:
            assert data[offset + 16] == 10 and data[offset + 20] == 255
            assert data[offset + 21:offset + 23] == bytes(2)
            aperture.append((x, y, z))
    assert len(aperture) >= 24, "expected the measured rear eyepiece ring"
    # The rendered circle is inset within the lip and slightly eye-side of it.
    center = (63.45, 39.8, 80.5)
    radial = [((x-center[0])**2 + ((z-center[2])*.985-(y-center[1])*.172)**2)**.5
              for x, y, z in aperture]
    assert min(radial) < 5.5 and max(radial) > 7.0
    assert all(abs(x-center[0]) < 7.5 for x, y, z in aperture)
    print(f"OK: {path.name}: {len(aperture)} aperture vertices, full joint-10 weighting")

root = Path(sys.argv[1]) / "4" / "ITEM"
for variant in ("BARE", "GLOVES", "XRAY"):
    verify(root / f"HAND_{variant}_HK.TRM")
