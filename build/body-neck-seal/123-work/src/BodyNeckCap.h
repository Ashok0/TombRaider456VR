#pragma once
#include "WristCap.h"
#include <cstdint>

namespace tr::bodyneckcap {
using namespace motiongun;
using Vertex=wristcap::Vertex;
inline float Weight(const Vertex& v,uint64_t mask) {
    float result=0;
    for(int k=0;k<3;++k) if(v.joint[k]>=0 && v.joint[k]<33 &&
        (mask&(uint64_t(1)<<v.joint[k]))) result+=v.weight[k];
    return result;
}
struct Deformation {
    float bone[4]{},region[12]{};
    static float Smooth(float a,float b,float x) {
        const float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);
    }
    Vec Apply(const Vertex& v,Vec p) const {
        if(bone[3]<=.5f) return p;
        const float* r=region;const auto q=v.position;const float soft=std::max(r[2],1.f);
        const float height=Smooth(r[0]-soft,r[0]+soft,q.y)*(1-Smooth(r[1]-soft,r[1]+soft,q.y));
        const float depth=Smooth(0,std::max(r[6],1.f),r[4]*(q.z-r[5]));
        const float lateral=1-Smooth(r[9],std::max(r[10],r[9]+1),std::fabs(q.x-r[8]));
        const int joint=int(r[3]+.5f);const float weight=joint>=0 && joint<33 ? Weight(v,1ull<<joint) : 0;
        return Add(p,Scale({bone[0],bone[1],bone[2]},weight*height*depth*lateral));
    }
};
inline Vec Skin(const Vertex& v,const float* palette,int count,const Deformation& deformation={}) {
    Vec result{};
    for(int k=0;k<3;++k) if(v.joint[k]>=0 && v.joint[k]<count && v.weight[k]>0)
        result=Add(result,Scale(Transform(ReadRows(palette+12*v.joint[k]),v.position),v.weight[k]));
    return deformation.Apply(v,result);
}
// Store the two original vertices, not an interpolated bind-space vertex.
// The fragment cutoff interpolates their SKINNED positions; blending weights
// first would move the rim away from the rendered seam as the neck bends.
struct Sample {
    Vertex a{},b{};
    float t=0;
    Vec Position(const float* palette,int count,const Deformation& deformation={}) const {
        const auto p=Skin(a,palette,count,deformation);
        return Add(p,Scale(Sub(Skin(b,palette,count,deformation),p),t));
    }
};
using Loop=std::vector<Sample>;
using Key=std::array<long long,3>;
inline Key PointKey(Vec p) { return {std::llround(p.x*1000.),std::llround(p.y*1000.),std::llround(p.z*1000.)}; }
class Builder {
    wristcap::Boundary boundary;
    std::map<Key,Sample> samples;
public:
    void AddTriangle(const Vertex (&v)[3],uint64_t hidden) {
        for(const auto& vertex:v) {
            const auto p=vertex.position;
            if(!std::isfinite(p.x+p.y+p.z) || std::fabs(p.x)>1e7f || std::fabs(p.y)>1e7f || std::fabs(p.z)>1e7f) return;
            for(float w:vertex.weight) if(!std::isfinite(w) || w<0 || w>1) return;
        }
        Vertex clipped[3]{};
        for(int i=0;i<3;++i) {
            const auto& a=v[i];const auto& b=v[(i+1)%3];
            const float wa=1-Weight(a,hidden),wb=1-Weight(b,hidden);
            clipped[i].position=a.position;clipped[i].joint[0]=0;clipped[i].weight[0]=wa;
            samples.emplace(PointKey(a.position),Sample{a,a,0});
            if((wa<.5f)!=(wb<.5f)) {
                const float t=(.5f-wa)/(wb-wa);
                samples.emplace(PointKey(Add(a.position,Scale(Sub(b.position,a.position),t))),Sample{a,b,t});
            }
        }
        boundary.Add(clipped,0);
    }
    std::vector<Loop> Finish(uint64_t body) const {
        std::vector<Loop> result;
        for(const auto& rim:boundary.Loops()) {
            Loop loop;bool valid=true;
            for(auto p:rim) {
                const auto found=samples.find(PointKey(p));
                if(found==samples.end()) { valid=false;break; }
                const auto& s=found->second;
                // Never cap a wrist, weapon, or arm attachment opening.
                if(Weight(s.a,body)*(1-s.t)+Weight(s.b,body)*s.t<.99f) { valid=false;break; }
                loop.push_back(s);
            }
            if(valid && loop.size()>=3) result.push_back(std::move(loop));
        }
        return result;
    }
};
// Close the nearest small body opening beneath the eye. Other mesh boundaries
// (boots, waist, backpack, separate accessories) are not neck openings.
inline std::vector<Vec> Triangles(const std::vector<Loop>& loops,const float* palette,int count,Vec eye,float units,const Deformation& deformation={}) {
    std::vector<Vec> chosen;Vec centre{};float nearest=units*units*.65f*.65f;
    for(const auto& loop:loops) {
        std::vector<Vec> points;Vec mid{};bool valid=true;
        for(const auto& s:loop) {
            const auto p=s.Position(palette,count,deformation);
            if(!std::isfinite(p.x+p.y+p.z)) { valid=false;break; }
            points.push_back(p);mid=Add(mid,p);
        }
        if(!valid || points.size()<3) continue;
        mid=Scale(mid,1.f/float(points.size()));
        const auto delta=Sub(mid,eye);const float distance=Dot(delta,delta);
        if(delta.y<units*.02f || distance>=nearest) continue;
        float radius=0;
        for(auto p:points) radius=std::max(radius,std::sqrt(Dot(Sub(p,mid),Sub(p,mid))));
        if(radius<units*.015f || radius>units*.18f) continue;
        chosen=std::move(points);centre=mid;nearest=distance;
    }
    std::vector<Vec> triangles;
    for(size_t i=0;i<chosen.size();++i) {
        triangles.push_back(centre);triangles.push_back(chosen[i]);triangles.push_back(chosen[(i+1)%chosen.size()]);
    }
    return triangles;
}
}
