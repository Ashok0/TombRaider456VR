#include "FirstPersonStabilization.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::stabilization;
static int checks=0;
static void Check(bool ok,const char* label) {
    ++checks;if(!ok) { std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1); }
}
static bool Near(Point a,Point b,float epsilon=.002f) {
    return std::fabs(a.x-b.x)<epsilon && std::fabs(a.y-b.y)<epsilon && std::fabs(a.z-b.z)<epsilon;
}
static Point Add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static Point Sub(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static Point Scale(Point a,float t) { return {a.x*t,a.y*t,a.z*t}; }
static Point Rotate(Point p,float yaw) {
    const auto flat=tr::locomotion::Rotate({p.x,p.z},yaw);return {flat.x,p.y,flat.z};
}
static void Sequence(float hz,float yaw,Point difference) {
    MountEyeTransition camera;
    const Point target{4000,-1800,-3000},last=Add(target,Rotate(difference,yaw));
    auto eye=camera.Apply(last,true,false,1,1,yaw,1024);camera.Observe(eye);
    Check(Near(eye,last),"mount animation is not filtered");
    eye=camera.Apply(target,false,true,2,1.01,yaw,1024);camera.Observe(eye);
    Check(Near(eye,last),"standing handoff begins at the final mounted eye");
    const auto duplicate=camera.Apply(target,false,true,2,1.015,yaw,1024);
    Check(Near(eye,duplicate),"repeated eye/attachment draw uses one weight");
    Point previousCorrection=Sub(last,target);
    for(int i=1;i<=int(hz*.3f)+1;++i) {
        const double elapsed=i/double(hz);
        const Point travel{float(elapsed)*100,0,float(elapsed)*160};
        const Point movingTarget=Add(target,travel);
        eye=camera.Apply(movingTarget,false,true,2+i,1.01+elapsed,yaw,1024);
        const float t=float(std::clamp(elapsed/.18,0.,1.));
        const float weight=1-t*t*t*(10+t*(-15+6*t));
        const Point correction=Scale(Rotate(difference,yaw),weight);
        Check(Near(eye,Add(movingTarget,correction)),"blend follows elapsed time while locomotion stays immediate");
        const auto distance=[](Point p) { return p.x*p.x+p.y*p.y+p.z*p.z; };
        Check(distance(correction)<=distance(previousCorrection)+.01f,"camera correction decays without overshoot");
        const Point hmd{float(i)*1.7f,float(i)*-.9f,float(i)*.4f};
        Check(Near(Sub(Add(eye,hmd),eye),hmd),"HMD translation added after handoff remains one-to-one");
        // Collision may retract the displayed eye, but cannot become the next
        // blend target and generate a feedback tail.
        camera.Observe(Add(eye,{20,-10,5}));previousCorrection=correction;
    }
    Check(!camera.active && Near(eye,Add(target,{float((int(hz*.3f)+1)/double(hz))*100,0,
        float((int(hz*.3f)+1)/double(hz))*160})),"handoff ends exactly without lingering drift");
}
int main() {
    for(float hz:{30.f,72.f,90.f,120.f,144.f}) for(float yaw:{-2.8f,-.3f,0.f,1.7f})
    for(Point delta:{Point{100,250,-80},Point{-60,-400,120},Point{0,0,0}}) Sequence(hz,yaw,delta);
    MountEyeTransition camera;
    const Point mount{20,-500,80},standing{0,-700,140};
    const auto begin=[&]() {
        camera.Reset();camera.Apply(mount,true,false,1,1,0,1024);camera.Observe(mount);
        return camera.Apply(standing,false,true,2,1.01,0,1024);
    };
    begin();
    auto eye=camera.Apply(standing,false,true,3,1.1,0,1024);
    Check(Near(eye,Scale(Add(mount,standing),.5f)),"180ms curve reaches exact midpoint at 90ms");
    const auto delta=Sub(eye,standing);
    eye=camera.Apply(standing,false,true,3,1.101,1.2f,1024);
    Check(Near(Sub(eye,standing),Rotate(delta,1.2f)),"snap turn transports correction in artificial yaw space");
    begin();
    const Point jump{10,-900,160};
    eye=camera.Apply(jump,false,false,3,1.04,0,1024);
    Check(Near(eye,jump) && !camera.active && !camera.pending,"jump immediately cancels camera handoff");
    eye=camera.Apply(standing,false,true,4,1.05,0,1024);
    Check(Near(eye,standing),"ordinary landing cannot revive mount handoff");
    begin();
    const Point nextMount{100,-1000,150};
    eye=camera.Apply(nextMount,true,false,3,1.04,0,1024);camera.Observe(eye);
    Check(Near(eye,nextMount) && !camera.active && camera.pending,"new climb owns the camera immediately");
    eye=camera.Apply(standing,false,true,3,1.041,0,1024);
    Check(Near(eye,nextMount),"same-frame completion uses the new climb endpoint");
    begin();camera.Reset();
    Check(Near(camera.Apply(standing,false,true,3,1.04,0,1024),standing),"recenter/level/session reset removes residual");
    begin();
    Check(Near(camera.Apply(standing,false,true,3,2,0,1024),standing) && !camera.active,"long pause discards stale correction");
    begin();
    Check(Near(camera.Apply(standing,false,true,3,.5,0,1024),standing),"clock reversal resets correction");
    camera.Reset();camera.Apply(mount,true,false,1,1,0,1024);camera.Observe(mount);
    const Point teleport{4000,-700,140};
    Check(Near(camera.Apply(teleport,false,true,2,1.01,0,1024),teleport) && !camera.active,"large relocation is never eased through geometry");
    camera.Reset();
    Check(Near(camera.Apply(standing,false,true,1,1,0,1024),standing),"ordinary standing never enables camera smoothing");
    camera.Apply(mount,true,false,2,1.01,0,1024);
    camera.Observe({NAN,0,0});
    Check(!camera.haveLast && !camera.pending,"invalid camera data discards history");
    std::printf("Mount camera: %d checks passed\n",checks);
}
