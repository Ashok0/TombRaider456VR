from pathlib import Path
for path in [Path('README.md'),Path('build/hands-out-capture/123-work/README.md')]:
 s=path.read_text(encoding='utf-8')
 old='''movement transport, distance thresholds and timing checks. Lowered-hand
movement detection retains its previous behavior.'''
 new='''movement transport, distance thresholds and timing checks.
Downward side inspections also check for a leaned torso swivelling around the
hips. A long, shared head-and-hand arc is not enough to release the planted
position or body heading. Straight shared steps have a separate fast path;
while swivelling, sustained movement of the inferred rotation centre can also
release the hold. This is still an estimate from three tracked devices:
walking while simultaneously leaning and swivelling can take longer to detect
than an ordinary straight step.'''
 assert old in s,path
 path.write_text(s.replace(old,new),encoding='utf-8')
