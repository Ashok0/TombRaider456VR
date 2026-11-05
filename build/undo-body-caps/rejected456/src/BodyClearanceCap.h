#pragma once
#include "BodyNeckCap.h"

namespace tr::bodyclearancecap {
using namespace motiongun;
using Triangle=std::array<wristcap::Vertex,3>;
struct Point { Vec p{};float body=0,visible=0; };
inline std::vector<Point> Clip(const std::vector<Point>& polygon,bool visibility) {
    std::vector<Point> result;
    for(size_t i=0;i<polygon.size();++i) {
        const auto& a=polygon[i];const auto& b=polygon[(i+1)%polygon.size()];
        const float wa=visibility?a.visible:a.body,wb=visibility?b.visible:b.body;
        if(wa>=.5f) result.push_back(a);
        if((wa<.5f)!=(wb<.5f)) {
            const float t=(.5f-wa)/(wb-wa);
            result.push_back({Add(a.p,Scale(Sub(b.p,a.p),t)),a.body+(b.body-a.body)*t,a.visible+(b.visible-a.visible)*t});
        }
    }
    return result;
}
// The shader keeps body fragments below a horizontal clearance plane. Extract
// only that plane's intersection segments, then join closed loops. No readback
// or full-mesh topology rebuild occurs per frame; only crossing triangles enter
// the boundary graph. An opaque cap makes the trimmed chest a closed surface.
inline std::vector<Vec> Triangles(const std::vector<Triangle>& mesh,const float* palette,int count,
                                 uint64_t body,uint64_t visible,Vec eye,float clearance,const bodyneckcap::Deformation& deformation={}) {
    const float plane=eye.y+clearance;
    wristcap::Boundary boundary;
    const Vec hub{eye.x+12345,plane,eye.z+23456};
    for(const auto& triangle:mesh) {
        Point points[3]{};int index=0;bool finite=true;
        float low=INFINITY,high=-INFINITY;
        for(const auto& v:triangle) {
            const auto p=bodyneckcap::Skin(v,palette,count,deformation);
            const float bw=bodyneckcap::Weight(v,body),vw=bodyneckcap::Weight(v,visible);
            if(!std::isfinite(p.x+p.y+p.z+bw+vw)) { finite=false;break; }
            points[index++]={p,bw,vw};
            low=std::min(low,p.y);high=std::max(high,p.y);
        }
        if(!finite || low>=plane || high<plane) continue;
        std::vector<Point> polygon(points,points+3);
        polygon=Clip(Clip(polygon,true),false);
        std::vector<Vec> ends;
        for(size_t i=0;i<polygon.size();++i) {
            const auto a=polygon[i].p,b=polygon[(i+1)%polygon.size()].p;
            if((a.y<plane)!=(b.y<plane)) {
                auto p=Add(a,Scale(Sub(b,a),(plane-a.y)/(b.y-a.y)));p.y=plane;ends.push_back(p);
            }
        }
        if(ends.size()!=2) continue;
        wristcap::Vertex fan[3]{};
        for(int i=0;i<3;++i) {fan[i].position=i==2?hub:ends[i];fan[i].weight[0]=1;}
        boundary.Add(fan,0);
    }
    std::vector<Vec> result;
    for(const auto& loop:boundary.Loops()) {
        Vec mid{};bool valid=true;
        for(auto p:loop) {
            // An open contour retains the artificial hub; never fill it.
            if(Dot(Sub(p,hub),Sub(p,hub))<1) valid=false;
            mid=Add(mid,p);
        }
        if(!valid || loop.size()<3) continue;
        mid=Scale(mid,1.f/float(loop.size()));
        for(size_t i=0;i<loop.size();++i) {
            result.push_back(mid);result.push_back(loop[i]);result.push_back(loop[(i+1)%loop.size()]);
        }
    }
    return result;
}
}
