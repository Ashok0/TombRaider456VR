from pathlib import Path
import json,hashlib,shutil
root=Path.cwd();dest=root/'build/swivel-rollback';old=root/'build/glance-3d-analysis';failed=root/'build/hands-out-capture'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
oldv=json.loads((old/'validation.json').read_text());newv=json.loads((failed/'validation.json').read_text())
source=root/'src/ControllerBodyAnchor.h'
assert sha(source)==newv['productionHashes']['src/ControllerBodyAnchor.h']
before=(failed/'before.h').read_bytes();assert hashlib.sha256(before).hexdigest().upper()==oldv['sources456']['src/ControllerBodyAnchor.h']
changes={'src/ControllerBodyAnchor.h':before}
path=root/'tools/body_transition_regression.inl'
s=path.read_text().replace('#include "recorded_hands_out_regression.inl"\n','').replace('    TestRecordedHandsOutSwivels();\n','')
changes['tools/body_transition_regression.inl']=s.replace('\n','\r\n').encode()
path=root/'README.md';s=path.read_text(encoding='utf-8')
new='''movement transport, distance thresholds and timing checks.
Downward side inspections also check for a leaned torso swivelling around the
hips. A long, shared head-and-hand arc is not enough to release the planted
position or body heading. Straight shared steps have a separate fast path;
while swivelling, sustained movement of the inferred rotation centre can also
release the hold. This is still an estimate from three tracked devices:
walking while simultaneously leaning and swivelling can take longer to detect
than an ordinary straight step.'''
assert new in s
s=s.replace(new,'''movement transport, distance thresholds and timing checks. Lowered-hand
movement detection retains its previous behavior.''')
changes['README.md']=s.replace('\n','\r\n').encode('utf-8')
for name,data in changes.items():
 assert hashlib.sha256(data).hexdigest().upper()==oldv['sources456'][name],name
for name,data in changes.items():
 backup=dest/'failed-source'/name;backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,backup)
 (root/name).write_bytes(data)
for name,expected in [('src/PlantedSwivel.h',newv['productionHashes']['src/PlantedSwivel.h']),('tools/recorded_hands_out_regression.inl','44EDE8884763F17FF763FE77867538FC1348721F189231EA136EB5DDF2CDCB45')]:
 path=root/name;assert sha(path)==expected,name
 backup=dest/'failed-source'/name;backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,backup);path.unlink()
print('Restored TR4-6 source, tests and README to the exact previous release hashes.')
