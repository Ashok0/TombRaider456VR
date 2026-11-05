"""Offline audit of both shipped restroom mirrors, without executing game code."""
from pathlib import Path
import hashlib
import json
import re
import sys
from mirror_asset_audit import Trg, ROOT456

root = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT456
trg = Trg(root / '5/DATA/RICH2.TRG')
header = (Path(__file__).resolve().parents[1]/'src/MirrorReflectionMath.h').read_text()
assert len(trg.rooms) == int(re.search(r'RenderRooms=(\d+)', header).group(1)) == 180
profiles = [
    (134, 32768, 72704, 3989463, 36, -24930, -24558,
     [(3453, 3715), (4475, 4734), (5500, 5761)], [4, 4, 4]),
    (135, 29696, 74752, 4026003, 48, -24961, -24591,
     [(2431, 2683), (3450, 3713), (4478, 4734)], [4, 4, 8]),
]
records = []
for room, origin_x, origin_z, start, count, top, bottom, panes, expected_counts in profiles:
    chunks = trg.rooms[room]
    assert len(chunks) == 1 and chunks[0][0] == 0
    assert chunks[0][-2:] == (start, count)
    counts, areas = [0, 0, 0], [0., 0., 0.]
    for face in trg.faces(room):
        a, b, c = face['positions']
        assert all(v[0] == 1024 and top <= v[1] <= bottom for v in (a, b, c))
        pane = next(i for i, (lo, hi) in enumerate(panes)
                    if all(lo <= v[2] <= hi for v in (a, b, c)))
        counts[pane] += 1
        areas[pane] += abs((b[1]-a[1])*(c[2]-a[2]) - (b[2]-a[2])*(c[1]-a[1])) / 2
    assert counts == expected_counts
    assert areas == [(hi-lo)*(bottom-top) for lo, hi in panes]
    compact = re.sub(r'\s+', '', header)
    prefix = f'{{{room},{origin_x},{origin_z},{start},{count},{origin_x+1024},{top},{bottom},'
    bounds = '{{' + '},{'.join(f'{lo+origin_z},{hi+origin_z}' for lo, hi in panes) + '}}'
    assert prefix + bounds + '}' in compact
    records.append(dict(room=room, glassTriangles=sum(counts), planeX=origin_x+1024,
                        top=top, bottom=bottom, worldZ=[[lo+origin_z, hi+origin_z] for lo, hi in panes]))
record = dict(asset=str(trg.path), sha256=hashlib.sha256(trg.data).hexdigest(), rooms=records)
out = Path('build/mirror')
out.mkdir(parents=True, exist_ok=True)
(out/'asset-audit.json').write_text(json.dumps(record, indent=2)+'\n')
print('Mirror asset audit passed: 6 exact rectangles in rooms 134/135, 28 triangles, no frame/sink geometry.')
