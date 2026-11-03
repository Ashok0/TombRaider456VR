from pathlib import Path
import hashlib
stage = Path(__file__).parent
target = Path(r'C:\dev\TombRaider123VR\README.md')
assert hashlib.sha256(target.read_bytes()).hexdigest() == (stage / 'README123-final.sha256').read_text()
target.write_bytes((stage / 'README123-final.md').read_bytes())
print('Updated TR1-3 climbing-transition documentation.')
