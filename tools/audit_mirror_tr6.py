"""Audit TR6's shipped horizontal reflection configuration without running the game."""
from pathlib import Path
import collections
import hashlib
import json
from mirror_asset_audit import ROOT456

path = ROOT456 / '6/AREA/ROOM_PLANES.TXT'
data = path.read_bytes()
entries, overwritten = {}, []
line_count = 0
for line in data.decode().splitlines():
    if not line.strip() or line.startswith('#'):
        continue
    level, room, height, gain = line.split()
    key = int(level), int(room)
    assert 0 <= key[0] < 35 and 0 <= key[1] < 192
    line_count += 1
    if key in entries:
        overwritten.append(key)
    entries[key] = dict(level=key[0], room=key[1], height=int(height), gain=float(gain))
record = dict(
    source=str(path), sha256=hashlib.sha256(data).hexdigest(), lines=line_count,
    uniqueRooms=len(entries), positiveGain=sum(r['gain'] > 0 for r in entries.values()),
    levels=dict(collections.Counter(r['level'] for r in entries.values())),
    overwritten=overwritten, entries=list(entries.values()),
    note='Native horizontal planar reflections configured at startup; zero gain entries still set enable=1. '
         'Last duplicate line wins. Native renderer includes room and character draws. '
         'No new wall-mirror patch applied. This is a configuration audit, not visual validation.')
Path('build/mirror/tr6-native-planes.json').write_text(json.dumps(record, indent=2) + '\n')
print(f"TR6: {line_count} configuration lines, {len(entries)} unique rooms, "
      f"{record['positiveGain']} positive gain, {len(record['levels'])} level IDs")
