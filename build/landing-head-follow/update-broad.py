from pathlib import Path
p=Path('tools/first_person_tests.cpp');s=p.read_text();s=s.replace('''    Check(joint==14,"stabilized eye still queries native head joint");''','''    Check(joint==14 || joint==0,"landing eye queries native head and scale pivot");''')
s=s.replace('''Check(camera.y_pos==standingY+drop,
                      "calibrated jumps and falls retain collision-relative eye height throughout landing recovery");''','''Check(camera.y_pos==standingY+drop+(ordinary?0:dip),
                      "ordinary jumps stay stable while hard-fall landings follow the animated head");''')
s=s.replace('''                const bool stableLanding=stable && calibrated;
                const float expected=std::min((stableLanding ? saved.local.y : float(height))+tracking,-64.f);''','''                const float expected=std::min(float(height)+tracking,-64.f);''').replace('calibrated landing rejects neck dip while tracking and floor clearance remain active','landing head height, physical tracking and floor clearance remain active')
a=s.index('                if (stableLanding) {');b=s.index('                    PHD_VECTOR animated',a)
s=s[:a]+'''                if (height+tracking < -65) {
'''+s[b:];p.write_text(s)
