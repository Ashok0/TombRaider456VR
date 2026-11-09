from pathlib import Path
p=Path(__file__).parent
s=Path('build/glance-exact-capture/replay-candidate.cpp').read_text()
s=s.replace('tr::firstperson::ControllerBodyAnchor anchor;','tr::firstperson::ControllerBodyAnchor anchor;\n    tr::firstperson::BodyFollower follower;float yawError=0;')
s=s.replace('{anchor.Reset();continue;}','{anchor.Reset();follower.Reset();continue;}')
needle='''        anchor.Update(frame,time,eye,left,right,base,body,tracked!=0,walking!=0,yaw,neck,forward);'''
replacement='''        const float predicted=follower.valid ? tr::firstperson::BodyAngle(follower.yaw-base) : body;
        if(walking) {follower.Reset();}
        else yawError=std::max(yawError,std::fabs(tr::firstperson::BodyAngle(predicted-body)));
        anchor.Update(frame,time,eye,left,right,base,walking ? body : predicted,tracked!=0,walking!=0,yaw,neck,forward);
        follower.Update(frame,time,base,base+anchor.yaw,base+yaw,{},{},walking!=0,tr::firstperson::BodyFollowSettings(),
            {},false,true,0,anchor.held,anchor.valid,
            anchor.heading.active ? base+anchor.heading.target : NAN,anchor.rotating);
        std::printf("YAW,%llu,%.9g,%.9g\\n",frame,body,predicted);'''
assert needle in s;s=s.replace(needle,replacement)
s=s.replace('std::fclose(file);','std::fclose(file);std::printf("Maximum pre-pose body yaw error %.6f deg\\n",yawError*57.2957795f);')
(p/'replay-loop.cpp').write_text(s)
cmd='''@echo off
call "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc build\\stance-pivot-review\\replay-loop.cpp /Febuild\\stance-pivot-review\\loop-baseline.exe /Fobuild\\stance-pivot-review\\loop-baseline.obj
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ibuild\\stance-pivot-review\\src /Isrc build\\stance-pivot-review\\replay-loop.cpp /Febuild\\stance-pivot-review\\loop-candidate.exe /Fobuild\\stance-pivot-review\\loop-candidate.obj
'''
(p/'build-loop.cmd').write_text(cmd)
