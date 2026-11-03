// Exercises the actual background detour with fake engine calls and real
// OpenGL colour/depth buffers; no game or headset is required.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#define private public
#include "../src/InlineHook.h"
#undef private
#include "../src/InventoryScene.cpp"

namespace {
int checks=0, nativeCalls=0, roomCalls=0, flushCalls=0;
bool active=true, recurse=false;
tr::Config config;
tr::GameDllLayout dll{};
uint64_t moduleBase=0;
alignas(16) unsigned char roomMemory[304*2]{};
unsigned char* rooms=roomMemory;
int16_t roomCount=2;
alignas(16) unsigned char camera[112]{};
void Check(bool ok,const char* label) {
    ++checks;
    if(!ok) { std::printf("FAIL: %s\n",label);std::exit(1); }
}
void __cdecl Native() { ++nativeCalls; }
void __cdecl DrawRooms(int room) {
    ++roomCalls;
    Check(tr::InventorySceneDrawing(),"scene uses gameplay stereo camera during native draws");
    Check(room==1,"native scene receives camera room");
    for(int i=0;i<2;++i) {
        auto& bits=*reinterpret_cast<uint16_t*>(roomMemory+i*304+112);
        Check(bits==0xA5FC,"both room draw bits reset without changing unrelated flags");
        bits|=3;
    }
    if(recurse) tr::inventoryscene::DrawBackground();
    // Native drawing fills both eye halves and leaves a restricted scissor
    // and disabled depth writes, as the game's last material can do.
    glEnable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);
    glScissor(0,0,8,8);glClearColor(1,0,0,1);glClearDepth(.25);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glScissor(8,0,8,8);glClearColor(0,1,0,1);glClearDepth(.5);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glClearDepth(1);glDepthMask(GL_FALSE);
}
void __cdecl Flush() {
    ++flushCalls;
    Check(tr::InventorySceneDrawing(),"queued scene draws flush before switching back to menu camera");
    glFlush();
}
uint32_t Rva(const void* p) {
    const auto delta=reinterpret_cast<uint64_t>(p)-moduleBase;
    Check(delta<=UINT32_MAX,"fixture address fits engine RVA");
    return uint32_t(delta);
}
}
void Log(const char*) {}
void LogF(const char*,...) {}
namespace hook {
InlineHook::~InlineHook() {}
bool InlineHook::Install(void*,void*,size_t,const uint8_t*,size_t,const char*,const int*,size_t) { return false; }
void InlineHook::Remove() { m_installed=false; }
}
namespace tr {
const Config& Cfg() { return config; }
const GameDllLayout* GameDllBound() { return &dll; }
uint64_t GameDllBase() { return moduleBase; }
bool WorldInventoryActive() { return active; }
}
int main() {
    WNDCLASSA wc{};wc.lpfnWndProc=DefWindowProcA;wc.hInstance=GetModuleHandle(nullptr);
    wc.lpszClassName="InventorySceneGLTest";wc.style=CS_OWNDC;
    Check(RegisterClassA(&wc)!=0,"register hidden GL window");
    const HWND window=CreateWindowA(wc.lpszClassName,"",WS_POPUP,0,0,16,8,nullptr,nullptr,wc.hInstance,nullptr);
    Check(window!=nullptr,"create hidden GL window");
    const HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pf{};pf.nSize=sizeof(pf);pf.nVersion=1;
    pf.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;pf.iPixelType=PFD_TYPE_RGBA;
    pf.cColorBits=24;pf.cDepthBits=24;
    const int format=ChoosePixelFormat(dc,&pf);
    Check(format && SetPixelFormat(dc,format,&pf),"create colour and depth buffers");
    const auto context=wglCreateContext(dc);
    Check(context && wglMakeCurrent(dc,context),"activate real OpenGL context");
    glViewport(0,0,16,8);
    moduleBase=reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
    dll.camera=Rva(camera);dll.room=Rva(&rooms);dll.numberRooms=Rva(&roomCount);
    *reinterpret_cast<int16_t*>(camera+12)=1;
    tr::inventoryscene::Build build{0,0,Rva(reinterpret_cast<void*>(&DrawRooms)),Rva(reinterpret_cast<void*>(&Flush)),0,0};
    tr::inventoryscene::base=moduleBase;tr::inventoryscene::layout=&dll;tr::inventoryscene::build=&build;
    tr::inventoryscene::backgroundHook.m_installed=true;
    tr::inventoryscene::backgroundHook.m_trampoline=reinterpret_cast<void*>(&Native);
    Check(tr::InventorySceneReady(),"matching hooked module is ready");
    for(int i=0;i<2;++i) *reinterpret_cast<uint16_t*>(roomMemory+i*304+112)=0xA5FF;
    for(int frame=0;frame<3;++frame) {
        tr::inventoryscene::DrawBackground();
        Check(roomCalls==frame+1 && flushCalls==frame+1 && nativeCalls==0,"redraw every frame without native backdrop");
        Check(!tr::InventorySceneDrawing(),"menu camera restored after scene flush");
        for(int eye=0;eye<2;++eye) {
            unsigned char colour[4]{};float depth=0;
            glReadPixels(eye*8+4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,colour);
            glReadPixels(eye*8+4,4,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
            Check(colour[eye]>240 && colour[1-eye]<10 && colour[2]<10,"world colour survives under menu in both eyes");
            Check(depth>.999f,"both eyes have fresh menu depth despite scene scissor and write mask");
        }
        GLboolean mask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
        GLint box[4]{};glGetIntegerv(GL_SCISSOR_BOX,box);
        Check(glIsEnabled(GL_SCISSOR_TEST) && !mask && box[0]==8 && box[2]==8,"native GL state restored");
        Check(glGetError()==GL_NO_ERROR,"render pass has no GL errors");
    }
    for(bool scissor:{false,true}) for(bool mask:{false,true}) {
        if(scissor) glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        glDepthMask(mask?GL_TRUE:GL_FALSE);tr::inventoryscene::ClearMenuDepth();
        GLboolean actual=GL_FALSE;glGetBooleanv(GL_DEPTH_WRITEMASK,&actual);
        Check(bool(glIsEnabled(GL_SCISSOR_TEST))==scissor && bool(actual)==mask,"all incoming depth/scissor states restored");
    }
    for(int guard=0;guard<5;++guard) {
        active=guard!=0;rooms=guard==1?nullptr:roomMemory;
        roomCount=guard==2?0:guard==3?1025:2;
        *reinterpret_cast<int16_t*>(camera+12)=guard==4?-1:1;
        tr::inventoryscene::DrawBackground();
        Check(nativeCalls==guard+1 && roomCalls==3,"ordinary menus or missing level fall back without drawing rooms");
    }
    active=true;rooms=roomMemory;roomCount=2;*reinterpret_cast<int16_t*>(camera+12)=1;
    recurse=true;tr::inventoryscene::DrawBackground();
    Check(roomCalls==4 && nativeCalls==6 && !tr::InventorySceneDrawing(),"nested invocation uses original and restores drawing scope");
    ++moduleBase;Check(!tr::InventorySceneReady(),"old module binding cannot be used for camera preservation");--moduleBase;
    tr::InventorySceneShutdown();
    Check(!tr::InventorySceneReady() && !tr::inventoryscene::base && !tr::inventoryscene::layout,"shutdown removes hook and scene binding");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("Inventory scene: %d checks passed (real OpenGL colour/depth, repeated frames, fallback and lifecycle).\n",checks);
}
