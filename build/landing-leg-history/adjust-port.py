from pathlib import Path
p=Path('build/landing-leg-history/port.py');s=p.read_text().replace("  assert s.count(old)==1,(n,old)","  if n=='FirstPerson.cpp':old=old.replace('Wrap(', 'locomotion::Wrap(');new=new.replace('Wrap(', 'locomotion::Wrap(')\n  assert s.count(old)==1,(n,old)");p.write_text(s)
