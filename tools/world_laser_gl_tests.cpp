// Real GL 3.2 core rendering and read-only native-memory tests, no game required.
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <vector>
#include <limits>
#include "../src/StereoMath.h"
#define private public
#include "../src/VRSystem.h"
#undef private
#include "../src/WorldLaser.cpp"
#include "../src/GL.cpp"
#include "../src/StereoRenderer.cpp"

namespace {
tr::Config cfg;
tr::mat4 projections[2]{};
tr::Layout layout{};
uint8_t app[4096]{};
std::vector<uint8_t> memory(0x700000);
tr::GameDllLayout dll{};
int game=1,checks=0;
bool zoom=true;
bool equippedLaser=false;
uint64_t queryBase=0;
float headX=0;
int traceCalls=0,objectResult=999;
void* __cdecl QueryFloor(int32_t,int32_t,int32_t,int16_t* room) { *room=7;return nullptr; }
int32_t __cdecl QueryLOS(tr::worldlaser::Point* start,tr::worldlaser::Point* end) {
    ++traceCalls;end->x=200;end->y=0;end->z=1000;return 0;
}
int32_t __cdecl QueryObjects(tr::worldlaser::Point*,tr::worldlaser::Point*,tr::worldlaser::Point* hit,void**) {
    *hit={0,0,600,7,0};return objectResult;
}
void Check(bool ok,const char* label) { ++checks;if(!ok) { std::printf("FAIL: %s\n",label);std::exit(1); } }
GLint Get(GLenum key) { GLint value=0;glGetIntegerv(key,&value);return value; }
void Seed(const tr::worldlaser::Build& b) {
    std::fill(memory.begin(),memory.end(),0);
    auto instruction=[&](uint32_t at,uint32_t target,const std::vector<uint8_t>& bytes,int disp) {
        std::memcpy(memory.data()+at,bytes.data(),bytes.size());
        const int32_t offset=int32_t(target-at-uint32_t(bytes.size()));
        std::memcpy(memory.data()+at+disp,&offset,4);
    };
    instruction(b.sightCheck,b.sight,{0x44,0x39,0x3d,0,0,0,0},3);
    instruction(b.activeCheck,b.active,{0xc6,0x05,0,0,0,0,1},2);
    instruction(b.lightCheck,b.light,{0x66,0xc7,0x05,0,0,0,0,1,2},3);
    instruction(b.losCall,b.los,{0xe8,0,0,0,0},1);
    instruction(b.objectsCall,b.objects,{0xe8,0,0,0,0},1);
    memory[b.sight]=memory[b.active]=1;
    const tr::worldlaser::Ray ray{{-200,0,200},{200,0,1000}};
    std::memcpy(memory.data()+b.light+0x14,&ray.start,12);
    std::memcpy(memory.data()+b.light+0x2c,&ray.end,12);
    dll.timestamp=b.stamp;dll.game=game;tr::worldlaser::checkedBase=0;
}
}
void Log(const char* s) { std::puts(s); }
void LogF(const char* f,...) { va_list a;va_start(a,f);std::vprintf(f,a);va_end(a);std::puts(""); }
namespace tr {
const Config& Cfg() { return cfg; }
float LiveWorldUnitsPerMetre() { return 1000; }
mat4* Proj() { return projections; }
uint64_t Base() { return reinterpret_cast<uint64_t>(app); }
const Layout& L() { return layout; }
int CurrentGame() { return game; }
const GameDllLayout* GameDllBound() { return &dll; }
uint64_t GameDllBase() { return queryBase ? queryBase : reinterpret_cast<uint64_t>(memory.data()); }
bool IsOpticsZoomed() { return zoom; }
bool WeaponLaserAim(motiongun::Vec& start,motiongun::Vec& direction,int16_t& room) {
    start={-200,0,200};direction={.5f,0,1};room=1;return equippedLaser;
}
VRSystem& VR() { static VRSystem vr;return vr; }
Affine VRSystem::EyeView(Eye eye) const {
    Affine v=Affine::Identity();v.r[0][3]=(eye==Eye::Left ? 32.f : -32.f)-headX;return v;
}
void VRSystem::EyeProjection(Eye,float n,float f,mat4& p) const { BuildEyeProjection(p,-1,1,-1,1,n,f,true); }
bool CameraViewFrame(float r[3][3],float p[3]) {
    std::memset(r,0,9*sizeof(float));std::memset(p,0,3*sizeof(float));r[0][0]=r[1][1]=r[2][2]=1;return true;
}
}

int main() {
    using namespace tr;
    for(const auto& b:worldlaser::builds) {
        Seed(b);const auto original=memory;worldlaser::Ray ray{};
        Check(worldlaser::Verify(memory.data(),b),"valid instruction references accepted");
        Check(worldlaser::Read(memory.data(),b,ray) && ray.end.x==200 && ray.end.z==1000,"native final world endpoint read");
        Check(memory==original,"reading laser never mutates game memory or fires");
        memory[b.active]=0;Check(!worldlaser::Read(memory.data(),b,ray),"inactive laser hidden");memory[b.active]=1;
        memory[b.sight]=0;Check(!worldlaser::Read(memory.data(),b,ray),"binoculars do not produce a weapon laser");memory[b.sight]=1;
        memory[b.activeCheck+1]^=1;Check(!worldlaser::Verify(memory.data(),b),"mismatched native code rejected");
        const float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(memory.data()+b.light+0x2c,&nan,4);
        Check(!worldlaser::Read(memory.data(),b,ray),"invalid native endpoint rejected");
    }
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"HKScopeTest";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Scope test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    Check(window!=nullptr,"create hidden window");HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;
    Check(SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)!=0,"set pixel format");
    HGLRC context=wglCreateContext(dc);Check(context && wglMakeCurrent(dc,context),"create GL context");
    using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    auto create=reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
    Check(create!=nullptr,"GL 3.2 context creation available");
    const int attribs[]={0x2091,3,0x2092,2,0x9126,1,0};
    HGLRC core=create(dc,nullptr,attribs);Check(core!=nullptr,"create GL 3.2 core");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);Check(wglMakeCurrent(dc,core)!=0,"activate core context");
    std::printf("GL: %s\n",glGetString(GL_VERSION));Check(gl::Load(),"load GL entry points");
    cfg.eyeOffsetMode=3;cfg.perEyeProjection=1;cfg.perEyeView=true;cfg.duplicateDraws=true;
    VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;
    BuildEyeProjection(projections[1],-1,1,-1,1,16,32768,true);
    Check(Stereo().Create(512,512),"create stereo target");
    const auto& b=worldlaser::builds[3];Seed(b);
    auto clear=[&](double depth=1) {
        gl::BindFramebuffer(GL_FRAMEBUFFER,Stereo().fbo());glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,1);glClearDepth(depth);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    };
    auto pixels=[]() {
        std::vector<uint8_t> p(1024*512*4);gl::BindFramebuffer(GL_READ_FRAMEBUFFER,Stereo().fbo());
        glReadPixels(0,0,1024,512,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;
    };
    auto redCount=[&]() { const auto p=pixels();int n=0;for(size_t i=0;i<p.size();i+=4) n+=p[i]>100;return n; };
    clear();glViewport(7,8,220,210);glScissor(9,10,180,170);glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);glDepthFunc(GL_GREATER);glDepthMask(GL_TRUE);glDepthRange(.2,.8);
    glEnable(GL_BLEND);glEnable(GL_CULL_FACE);glColorMask(GL_FALSE,GL_TRUE,GL_TRUE,GL_FALSE);
    const auto drawBefore=Get(GL_DRAW_FRAMEBUFFER_BINDING);
    WorldLaserPresent();
    Check(Get(GL_DRAW_FRAMEBUFFER_BINDING)==drawBefore && Get(GL_CURRENT_PROGRAM)==0 && Get(GL_VERTEX_ARRAY_BINDING)==0 &&
          Get(GL_ARRAY_BUFFER_BINDING)==0,"laser restores framebuffer program VAO and VBO");
    GLint vp[4],sc[4];glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(GL_SCISSOR_BOX,sc);
    GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);GLdouble range[2];glGetDoublev(GL_DEPTH_RANGE,range);
    Check(vp[0]==7 && vp[1]==8 && sc[0]==9 && sc[1]==10 && !glIsEnabled(GL_SCISSOR_TEST),"viewport and scissor restored");
    Check(!glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && Get(GL_DEPTH_FUNC)==GL_GREATER &&
          !mask[0] && mask[1] && !mask[3] && std::fabs(range[0]-.2)<.001 && std::fabs(range[1]-.8)<.001,"render state restored");
    Check(redCount()>100,"world beam draws visible pixels");
    auto endpoint=[&](int eye,float offset) {
        const auto p=pixels();const int x=int(256+256*(200+(eye==0?32:-32)-offset)/1000)+eye*512;
        bool found=false;for(int y=254;y<=258;++y) for(int dx=-2;dx<=2;++dx) found|=p[(y*1024+x+dx)*4]>100;
        return found;
    };
    Check(endpoint(0,0) && endpoint(1,0),"dot occupies correct different per-eye world projections");
    headX=100;clear();WorldLaserPresent();Check(endpoint(0,100) && endpoint(1,100),"physical head lean changes parallax at fixed world endpoint");headX=0;
    // Exercise Present -> equipped weapon -> collision queries -> actual GPU,
    // with scope flags zero. Only the native geometry callbacks are replaced.
    auto queryBuild=b;queryBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    queryBuild.los=uint32_t(reinterpret_cast<uint64_t>(&QueryLOS)-queryBase);
    queryBuild.objects=uint32_t(reinterpret_cast<uint64_t>(&QueryObjects)-queryBase);
    dll.getFloor=uint32_t(reinterpret_cast<uint64_t>(&QueryFloor)-queryBase);
    worldlaser::native=&queryBuild;worldlaser::checkedBase=queryBase;
    zoom=false;equippedLaser=true;memory[b.sight]=memory[b.active]=0;
    clear();WorldLaserPresent();Check(redCount()>100 && traceCalls==1 && endpoint(0,0) && endpoint(1,0),
          "equipped laser renders both eyes with scope and native laser flags completely off");
    worldlaser::Ray ray{};objectResult=3;
    Check(worldlaser::ReadNativeRay(ray) && ray.end.x==0 && ray.end.z==600,"free beam stops at nearer object intersection");
    objectResult=999;equippedLaser=false;clear();WorldLaserPresent();Check(redCount()==0,"unequipping weapon immediately removes free laser");
    queryBase=0;zoom=true;Seed(b);
    clear(0);WorldLaserPresent();Check(redCount()==0,"nearer geometry occludes beam and endpoint");
    clear();WorldLaserPresent();
    float depth=0;glReadPixels(307,256,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);Check(depth==1,"laser does not write scene depth");
    for(int gate=0;gate<9;++gate) {
        clear();
        if(gate==0) zoom=false;
        if(gate==1) memory[b.sight]=0;
        if(gate==2) memory[b.active]=0;
        if(gate==3) cfg.worldLaserSight=false;
        if(gate==4) *reinterpret_cast<int32_t*>(app+drva::app_off::InventoryActive)=1;
        if(gate==5) *reinterpret_cast<int32_t*>(app+drva::app_off::InTitle)=1;
        if(gate==6) *reinterpret_cast<int32_t*>(app+drva::app_off::InFMV)=1;
        if(gate==7) game=2;
        if(gate==8) VR().m_poseValid=false;
        WorldLaserPresent();Check(redCount()==0,"laser hidden outside valid active weapon sight gameplay");
        zoom=true;memory[b.sight]=memory[b.active]=1;cfg.worldLaserSight=true;game=1;VR().m_poseValid=true;std::memset(app,0,sizeof(app));
    }
    worldlaser::Vertex vertices[12];const auto identity=Affine::Identity();
    Check(worldlaser::Mesh({{0,0,20},{0,0,-1000}},identity,1000,16,vertices)==12 && vertices[0].z<=-16,
          "beam starting behind eye clips safely to near plane");
    Check(worldlaser::Mesh({{0,0,20},{0,0,1000}},identity,1000,16,vertices)==0,"ray behind viewer is not drawn");
    WorldLaserShutdown();Stereo().Destroy();Check(glGetError()==GL_NO_ERROR,"no GL errors or cleanup errors");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(core);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("OK: %d world laser checks passed.\n",checks);
}
