from pathlib import Path
p=Path('build/landing-head-follow/123-work/README.md');s=p.read_text(encoding='utf-8');s=s.replace('''folding the shins through the floor. Tracked hands remain active. The calibrated
camera follows Lara's collision position through impact without the animated
neck dip, keeping the view above her legs. Physical head movement remains live;
collision, damage and recovery timing remain native.''','''folding the shins through the floor. Tracked hands remain active. The landing
camera follows the animated head at the visible body's scale, with the eye
offset in Lara's facing frame so head pitch cannot rotate it into her legs.
Physical head movement, collision, damage and recovery timing remain native.''');p.write_text(s,encoding='utf-8')
