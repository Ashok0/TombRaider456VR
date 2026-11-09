#pragma once
#include "BodyFollower.h"
namespace tr::firstperson {
// A downward inspection can swivel the leaned upper torso about the hips.
// Its long head orbit is not the short eye/neck orbit of an upright body turn.
// Use this only as evidence for retaining an existing stance. It never changes
// the tracked view, body position, or the displacement consumed by locomotion.
struct PlantedSwivel {
    struct Sample { Vec eye{},left{},right{};double time=0; };
    Sample samples[96]{};int next=0,count=0;
    double until=0;
    double velocitySince=-1,travelUntil=0,lastFit=0;Vec velocityDirection{};
    static bool Solve(double (&m)[6][7],double (&out)[6]) {
        for(int j=0;j<6;++j) {
            int pivot=j;for(int k=j+1;k<6;++k) if(std::fabs(m[k][j])>std::fabs(m[pivot][j])) pivot=k;
            if(std::fabs(m[pivot][j])<1.e-10) return false;
            for(int k=0;k<7;++k) std::swap(m[j][k],m[pivot][k]);
            const double scale=m[j][j];for(int k=j;k<7;++k) m[j][k]/=scale;
            for(int i=0;i<6;++i) if(i!=j) {
                const double f=m[i][j];for(int k=j;k<7;++k) m[i][k]-=f*m[j][k];
            }
        }
        for(int i=0;i<6;++i) out[i]=m[i][6];return true;
    }
    bool SharedTravel(double now) {
        if(now-lastFit>.08) velocitySince=-1;
        lastFit=now;
        const auto uncertain=[&]() {velocitySince=-1;return now<travelUntil;};
        // Fit head = moving centre + rotated lever, using the controller pair
        // for the angle. Pure swivel has a stationary centre; walking while
        // swivelling adds a shared linear term. Confidence includes the fit's
        // uncertainty, so near-singular short arcs do not authorize a step.
        double normal[6][7]{};int used=0;double earliest=now;
        for(int i=0;i<count;++i) {
            const auto& s=samples[i];if(now-s.time>.7) continue;
            const Vec pair=Sub(s.right,s.left);if(Length(Flat(pair))<.12f) continue;
            const double angle=std::atan2(pair.x,pair.z),c=std::cos(angle),sn=std::sin(angle),t=s.time-now;
            const double rows[2][7]={{1,0,t,0,c,sn,s.eye.x},{0,1,0,t,-sn,c,s.eye.z}};
            for(const auto& row:rows) for(int j=0;j<6;++j) for(int k=0;k<7;++k) normal[j][k]+=row[j]*row[k];
            ++used;earliest=std::min(earliest,s.time);
        }
        if(used<12 || now-earliest<.45) return uncertain();
        double matrix[6][7]{},fit[6]{};
        for(int j=0;j<6;++j) for(int k=0;k<7;++k) matrix[j][k]=normal[j][k];
        if(!Solve(matrix,fit) || std::hypot(fit[4],fit[5])>1.2) return uncertain();
        double error=0;
        for(int i=0;i<count;++i) {
            const auto& s=samples[i];if(now-s.time>.7) continue;
            const Vec pair=Sub(s.right,s.left);if(Length(Flat(pair))<.12f) continue;
            const double a=std::atan2(pair.x,pair.z),c=std::cos(a),sn=std::sin(a),t=s.time-now;
            const double x=fit[0]+fit[2]*t+c*fit[4]+sn*fit[5]-s.eye.x;
            const double z=fit[1]+fit[3]*t-sn*fit[4]+c*fit[5]-s.eye.z;error+=x*x+z*z;
        }
        const double variance=error/(2*used-6);if(variance>.035*.035) return uncertain();
        double uncertainty=0;
        for(int col:{2,3}) {
            for(int j=0;j<6;++j) {for(int k=0;k<6;++k) matrix[j][k]=normal[j][k];matrix[j][6]=j==col?1:0;}
            double inverse[6]{};if(!Solve(matrix,inverse)) return uncertain();
            uncertainty+=std::max(0.,inverse[col])*variance;
        }
        Vec velocity{float(fit[2]),0,float(fit[3])};const float speed=Length(velocity);
        if(speed<.14+2*std::sqrt(uncertainty)) {velocitySince=-1;return now<travelUntil;}
        const Vec direction=Scale(velocity,1/speed);
        if(velocitySince<0 || Dot(direction,velocityDirection)<.8f) velocitySince=now;
        velocityDirection=direction;
        if(now-velocitySince>.18) travelUntil=now+.15;
        return now<travelUntil;
    }
    bool Update(Vec eye,Vec left,Vec right,Vec forward,double now) {
        bool orbit=false,straight=false;
        // A shared straight step does not need a long angular fit. Keep this
        // fast path independent of gaze/pitch and of the previous hold timer.
        for(int i=0;i<count;++i) {
            const auto& s=samples[i];const double age=now-s.time;if(age<.08 || age>.12) continue;
            const Vec h=Flat(Sub(eye,s.eye)),l=Flat(Sub(left,s.left)),r=Flat(Sub(right,s.right));
            const float distance=Length(h);
            if(distance>.006f && Length(Sub(h,l))<distance*.12f && Length(Sub(h,r))<distance*.12f) straight=true;
        }
        if(forward.y<-.65f) {
            for(int i=0;i<count;++i) {
                const auto& s=samples[i];const double age=now-s.time;
                if(age<.10 || age>.24) continue;
                const Vec a=Flat(Sub(s.right,s.left)),b=Flat(Sub(right,left));
                if(Length(a)<.12f || Length(b)<.12f) continue;
                const float turn=BodyAngle(std::atan2(b.x,b.z)-std::atan2(a.x,a.z));
                if(std::fabs(turn)<.04f || std::fabs(turn)>.7f) continue;
                const Vec oldMid=Scale(Add(s.left,s.right),.5f),mid=Scale(Add(left,right),.5f);
                const Vec predicted=Add(mid,YawVector(Sub(s.eye,oldMid),turn));
                if(Length(Flat(Sub(predicted,eye)))>.06f) continue;
                const Vec t=Sub(mid,YawVector(oldMid,turn));
                const float c=1-std::cos(turn),sn=std::sin(turn),den=c*c+sn*sn;
                const Vec pivot{(c*t.x+sn*t.z)/den,eye.y,(-sn*t.x+c*t.z)/den};
                const float radius=Length(Flat(Sub(eye,pivot)));
                if(radius>.24f && radius<.85f) {orbit=true;break;}
            }
        }
        if(orbit) until=now+.12;
        // Bound both storage and work independently of headset refresh rate.
        if(count==0 || now-samples[(next+95)%96].time>=.01) {
            samples[next]={eye,left,right,now};next=(next+1)%96;count=std::min(96,count+1);
        }
        return forward.y<-.65f && now<until && !straight && !SharedTravel(now);
    }
};
}
