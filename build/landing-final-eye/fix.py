from pathlib import Path
r=Path.cwd();w=r/'build/landing-final-eye';stage=w/'123-work'
for base in [r,stage]:
 p=base/'src/FirstPersonStabilization.h';s=p.read_text();s=s.replace('''                            Point eyeOffset,float yaw) {''','''                            Point eyeOffset,float yaw,float trackedY=0) {''')
 s=s.replace('''            rootY+(head.y-rootY+eyeOffset.y)*scale,''','''            rootY+(head.y-rootY+eyeOffset.y)*scale-std::max(0.f,trackedY),''')
 s=s.replace('// down into the folded legs. Apply the same fixed scale/pivot as the palette.','''// down into the folded legs. Apply the same fixed scale/pivot as the palette.
// Stereo adds trackedY AFTER this anchor. During the authored impact crouch,
// limit extra downward travel to the animated eye height; otherwise a downward
// glance places the actual view below the head even with a correct anchor.
// Upward tracking remains live. Tracking/calibration themselves are unchanged.''');p.write_text(s)
 p=base/'src/FirstPerson.cpp';s=p.read_text();old='''            {float(Cfg().firstPersonAnchorX),float(Cfg().firstPersonAnchorY),float(Cfg().firstPersonAnchorZ)},yaw);'''
 new='''            {float(Cfg().firstPersonAnchorX),float(Cfg().firstPersonAnchorY),float(Cfg().firstPersonAnchorZ)},yaw,
            Cfg().positionalTracking && Cfg().firstPersonHeadTranslation'''+(' && !VR().headAtCamera()' if base==r else '')+''' ?
                VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre() : 0.f);'''
 assert s.count(old)==1;s=s.replace(old,new);p.write_text(s)
 p=base/'README.md';s=p.read_text(encoding='utf-8');find='''active. The calibrated camera follows Lara's collision position through impact
without the animated neck dip, keeping the view above her legs. Physical head
movement stays live; collision, damage and recovery timing remain native.''';repl='''active. The camera follows Lara's animated head at her visible body scale.
During that impact animation, downward headset displacement is limited at
Lara's animated eye height so the final view cannot sink below her head. Upward
head movement and looking around remain tracked. Collision, damage and recovery
timing remain native.'''
 if base==stage:
  find='''Physical head movement, collision, damage and recovery timing remain native.'''
  repl='''During impact, downward headset displacement is limited at the animated eye
height so the final view cannot sink below her head. Upward head movement and
looking around remain tracked. Collision, damage and recovery timing remain native.'''
 assert find in s;s=s.replace(find,repl);p.write_text(s,encoding='utf-8')
p=r/'tools/landing_camera_test.cpp';s=p.read_text();s=s.replace('for(float duck:{0.f,-.2f}) {','for(float duck:{0.f,-.17f,-.4f,.2f}) for(int tracking:{0,1,2,3}) {')
s=s.replace('''            config.firstPersonPhysicalCrouch=false;''','''            config.firstPersonPhysicalCrouch=false;
            config.positionalTracking=tracking!=1;config.firstPersonHeadTranslation=tracking!=2;
            VR().m_headAtCamera=tracking==3;''')
s=s.replace('''                const float y=stable?root+(head.y-root-32)*scale:raw.y;''','''                const float trackingY=tracking==0 ? duck*(config.flipViewY?-1.f:1.f)*LiveWorldUnitsPerMetre() : 0;
                const float y=stable?root+(head.y-root-32)*scale-std::max(0.f,trackingY):raw.y;''')
s=s.replace('''Check(!stable || std::fabs(camera.y_pos-(root+(head.y-root)*scale)+32*scale)<1.1f,''','''Check(!stable || std::fabs(camera.y_pos+std::max(0.f,trackingY)-(root+(head.y-root)*scale)+32*scale)<1.1f,''')
s=s.replace('''                Check(!stable || renderedY<=root+(head.y-root-32)*scale+1.1f,''','''                Check(std::fabs(InvertRigid(VR().HeadView()).r[1][3]-trackingY)<.01f,"test uses actual rendered tracking translation");
                Check(!stable || std::fabs(renderedY-(root+(head.y-root-32)*scale+std::min(0.f,trackingY)))<1.1f,''')
s=s.replace('''    VR().m_system=nullptr;''','''    VR().m_headAtCamera=false;VR().m_system=nullptr;''');p.write_text(s)
# Keep floor/render regression cameras representative of the actual landing anchor.
for p in [r/'tools/landing_floor_regression.inl',stage/'tests/landing_floor_selftest.h']:
 s=p.read_text();name='itemMemory' if base==r else 'item'
 needle='''            g_scenePose={int32_t(eye.x),int32_t(eye.y),int32_t(eye.z)};'''
 s=s.replace(needle,'''            const float landingTracking=s.clip==103 ? 0.f : std::max(0.f,VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre());
            g_scenePose={int32_t(eye.x),int32_t(std::lround(eye.y-landingTracking)),int32_t(eye.z)};''');p.write_text(s)
