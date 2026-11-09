import json
from pathlib import Path
p=Path('build/hands-out-capture');rows=[r for r in json.loads((p/'inputs.json').read_text()) if 1554<=r['frame']<=2200]
def f(x):
 s=format(x,'.9g');return s+('.0' if '.' not in s and 'e' not in s else '')+'f'
def vec(v):return '{'+','.join(map(f,v))+'}'
code='''#include "ControllerBodyAnchor.h"
#include <cstdio>
using namespace tr::firstperson;
struct Sample { unsigned long long frame;double time;float body,yaw;Vec eye,left,right,forward; };
const Sample samples[]={
'''
for r in rows:code+='{'+str(r['frame'])+','+repr(r['t'])+','+f(r['body'])+','+f(r['yaw'])+','+','.join(vec(r[k]) for k in ['h','l','r','f'])+'},\n'
code+='''};
int main() {
 for(int mode=0;mode<6;++mode) {
 ControllerBodyAnchor a;Vec initial{},shift{};float path=0;int turns=0,moves=0;
 for(const auto& s:samples) {
   Vec old=a.head;
   if(mode>=2) shift=YawVector({0,0,float(s.time-samples[0].time)*.25f},(mode-2)*1.5707963f);
   a.Update(s.frame,s.time,Add(s.eye,shift),Add(s.left,shift),Add(s.right,shift),0,mode==1?s.body:samples[0].body,true,false,s.yaw,.15f,s.forward);
   if(s.frame==samples[0].frame) initial=a.head;else path+=Length(Flat(Sub(a.head,old)));
   turns+=a.rotating;moves+=a.translating;
 }
 printf("mode %d path %.3f turn %d move %d travel %.3f expected %.3f\\n",mode,path,turns,moves,Length(Flat(Sub(a.head,initial))),Length(shift));
 if(mode==0) for(int dir=0;dir<4;++dir) {
   auto w=a;auto& s=samples[sizeof(samples)/sizeof(samples[0])-1];int first=0;
   Vec shift{};
   for(int i=1;i<=180;++i) {
     shift=YawVector({0,0,i*.0015f},dir*1.5707963f);
     w.Update(s.frame+i,s.time+i/90.,Add(s.eye,shift),Add(s.left,shift),Add(s.right,shift),0,samples[0].body,true,false,s.yaw,.15f,s.forward);
     if(!first && w.translating) first=i;
   }
   printf("walk-after %d first %d error %.4f\\n",dir,first,Length(Flat(Sub(Sub(w.head,a.head),shift))));
 }
 }
}
'''
(p/'new-recorded.cpp').write_text(code)
(p/'test-new.cmd').write_text('''@echo off
call "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\\hands-out-capture\\src /Isrc build\\hands-out-capture\\new-recorded.cpp /Febuild\\hands-out-capture\\new-recorded.exe /Fobuild\\hands-out-capture\\new-recorded.obj
if errorlevel 1 exit /b 1
build\\hands-out-capture\\new-recorded.exe
''')
