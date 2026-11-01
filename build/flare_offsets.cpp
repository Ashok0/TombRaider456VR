#include <windows.h>
#include <cstdio>
#include <cstddef>
#define private public
#include "VRSystem.h"
#undef private
int main(){
    using tr::VRSystem;
    std::printf("head %zu controllers %zu valid %zu posevalid %zu serial %zu neutral %zu\n",
        offsetof(VRSystem,m_rawHeadPose),offsetof(VRSystem,m_rawControllerPose),offsetof(VRSystem,m_controllerPoseValid),
        offsetof(VRSystem,m_poseValid),offsetof(VRSystem,m_poseSerial),offsetof(VRSystem,m_firstPersonNeutralValid));
}
