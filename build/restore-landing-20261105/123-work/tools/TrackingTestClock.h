#pragma once
#include <windows.h>
namespace trackingtest {
inline double time=-1;
inline BOOL Counter(LARGE_INTEGER* value) {
    if(time<0) return ::QueryPerformanceCounter(value);
    LARGE_INTEGER frequency{};::QueryPerformanceFrequency(&frequency);
    value->QuadPart=static_cast<LONGLONG>(time*double(frequency.QuadPart));return TRUE;
}
struct Clock {
    double saved=time;bool active=true;
    Clock() {
        if(time<0) {
            LARGE_INTEGER counter{},frequency{};
            ::QueryPerformanceCounter(&counter);::QueryPerformanceFrequency(&frequency);
            time=double(counter.QuadPart)/double(frequency.QuadPart);
        }
    }
    void Advance(double seconds) {time+=seconds;}
    void End() {if(active) time=saved;active=false;}
    ~Clock() {End();}
};
}
