from pathlib import Path
import shutil
r=Path.cwd(); w=r/'build/landing-head-follow'; stage=w/'123-work'; other=Path('C:/dev/TombRaider123VR')
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (other/name).exists(): shutil.copytree(other/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:shutil.copy2(other/name,stage/name)
for name in ['src/FirstPerson.cpp','src/FirstPersonStabilization.h','src/ControllerBodyAnchor.h','src/PlantedPivotEvidence.h','tools/landing_camera_test.cpp','tools/landing_floor_regression.inl','README.md']:
 p=w/'before'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r/name,p)
for name in ['build-456.cmd','build-123.cmd','test-arms-456.cmd','test-targeted-camera.cmd']:
 (w/name).write_text((r/'build/landing-camera-height'/name).read_text().replace('landing-camera-height','landing-head-follow'))
# Shared geometry: render fitting scales horizontal offsets about hip and height about collision root.
p=r/'src/FirstPersonStabilization.h';s=p.read_text();at='struct GroundEye {'
s=s.replace(at,'''// Follow the visible landing head, with the eye offset in Lara's yaw frame.
// Pitching the forward eye offset with the impact animation can push the view
// down into the folded legs. Apply the same fixed scale/pivot as the palette.
inline Point LandingHeadEye(Point head,Point hip,float rootY,float scale,
                            Point eyeOffset,float yaw) {
    const auto flat=locomotion::Rotate({eyeOffset.x*scale,eyeOffset.z*scale},yaw);
    return {hip.x+(head.x-hip.x)*scale+flat.x,
            rootY+(head.y-rootY+eyeOffset.y)*scale,
            hip.z+(head.z-hip.z)*scale+flat.z};
}

'''+at);p.write_text(s);shutil.copy2(p,stage/'src/FirstPersonStabilization.h')
p=r/'src/FirstPerson.cpp';s=p.read_text();s=s.replace('|| UseLandingCamera(item)) &&\n               LaraWaterStatus()',') &&\n               LaraWaterStatus()')
s=s.replace('''    } else if (Cfg().firstPersonMovementStabilization &&
               (state==3''','''    } else if (Cfg().firstPersonMovementStabilization && UseLandingCamera(item)) {
        PHD_VECTOR centre{},hip{};
        const auto query=reinterpret_cast<Fn_GetJointAbsPositionLerp>(g_boundBase+g_boundDll->getJointAbsPositionLerp);
        query(item,&centre,joint,frac);query(item,&hip,0,frac);
        const float yaw=Radians(old.y_rot)+Wrap(Radians(pos.y_rot)-Radians(old.y_rot))*t;
        const auto eye=stabilization::LandingHeadEye(
            {float(centre.x),float(centre.y),float(centre.z)},
            {float(hip.x),float(hip.y),float(hip.z)},body[1],AnimatedBodyScale(item),
            {float(Cfg().firstPersonAnchorX),float(Cfg().firstPersonAnchorY),float(Cfg().firstPersonAnchorZ)},yaw);
        pose.x_pos=int32_t(std::lround(eye.x));pose.y_pos=int32_t(std::lround(eye.y));pose.z_pos=int32_t(std::lround(eye.z));
    } else if (Cfg().firstPersonMovementStabilization &&
               (state==3''')
s=s.replace('''        // Keep the same calibrated eye through flight and landing. The native
        // impact kneel must not pull the headset down into the folded legs.
''','''        // Keep the calibrated eye through flight; landing follows the head above.
''');p.write_text(s)
p=stage/'src/FirstPerson.cpp';s=p.read_text();s=s.replace('''    if (Cfg().firstPersonMovementStabilization && (state==3 || state==9 || (state>=25 && state<=28) || UseLandingCamera(item)) &&''','''    if(Cfg().firstPersonMovementStabilization && UseLandingCamera(item)) {
        const auto head=NativeJointPoint(item,Cfg().firstPersonJoint),hip=NativeJointPoint(item,0);
        const float yaw=Radians(prev.y_rot)+Wrap(Radians(pos.y_rot)-Radians(prev.y_rot))*(frac/256.f);
        const auto eye=stabilization::LandingHeadEye({head.x,head.y,head.z},{hip.x,hip.y,hip.z},
            float(Lerp(prev.y_pos,pos.y_pos,frac)),AnimatedBodyScale(item),
            {float(Cfg().firstPersonAnchorX),float(Cfg().firstPersonAnchorY),float(Cfg().firstPersonAnchorZ)},yaw);
        pose.x_pos=int32_t(std::lround(eye.x));pose.y_pos=int32_t(std::lround(eye.y));pose.z_pos=int32_t(std::lround(eye.z));
        g_mountRootHeight.Reset();
    } else if (Cfg().firstPersonMovementStabilization && (state==3 || state==9 || (state>=25 && state<=28)) &&''')
s=s.replace('''        // Keep the calibrated eye through takeoff, flight and the impact kneel.
        // The landing neck must not pull the headset down into the folded legs.
''','');p.write_text(s)
for p in [r/'README.md',stage/'README.md']:
 s=p.read_text();s=s.replace('''active. The calibrated camera follows Lara's collision position through impact
without the animated neck dip, keeping the view above her legs. Physical head
movement stays live; collision, damage and recovery timing remain native.''','''active. The landing camera follows the animated head at the visible body's
scale. Its eye offset stays in Lara's facing frame, so impact head pitch cannot
rotate the forward offset down into her legs. Physical head movement stays live;
collision, damage and recovery timing remain native.''');p.write_text(s)
