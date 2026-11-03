from pathlib import Path
import hashlib, json
root = Path(r'C:\dev\TombRaider123VR')
stage = Path(__file__).parent
records = json.loads((stage / '123-edits.json').read_text())
for record in records:
    assert hashlib.sha256((root / record['path']).read_bytes()).hexdigest() == record['before'], record['path']
for record in records:
    (root / record['path']).write_bytes((stage / 'staged' / record['path']).read_bytes())
print('Applied four backed-up TR1-3 arm-visibility changes.')
