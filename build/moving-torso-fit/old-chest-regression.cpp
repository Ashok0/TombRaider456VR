// Exercise palette-relative chest direction and visible GPU deformation.
// TR1-3 builds define TR123_TORSO_TEST and use their own production headers.
#include "before456/src/DynamicBones.cpp"
#include "BoneSkin.cpp"
#include "TorsoClearance.h"
#include <cstdarg>
#include <cstdlib>
void Log(const char* s) { std::puts(s); }
void LogF(const char* f,...) { va_list a;va_start(a,f);std::vprintf(f,a);va_end(a);std::puts(""); }
int checks=0;
void Check(bool ok,const char* label) { ++checks;if(!ok) {std::printf("FAIL: %s\n",label);std::exit(1);} }
namespace tr {
uint64_t Base() { return 0; }
const Layout& L() { static Layout l;return l; }
bool IsWorldPass() { return true; }
alignas(8) unsigned char gameMemory[32]{},itemMemory[8192]{};
uint64_t GameDllBase() { return reinterpret_cast<uint64_t>(gameMemory); }
Config testConfig;
const Config& Cfg() { return testConfig; }
GameDllLayout testDll;
const GameDllLayout* GameDllBound() { return &testDll; }
RenderState& VidState() { static RenderState s;return s; }
Shader* Shaders() { static Shader s[202];return s; }
bool FirstPersonBodyClearance(float*,uint64_t&) { return false; }
#ifdef TR123_TORSO_TEST
const float* FirstPersonNativeBodySkin() { return nullptr; }
void FirstPersonDrawLara(void* item,void*,void (__fastcall* draw)(void*)) { draw(item); }
int CurrentGame() { return 1; }
int AppFlag(unsigned) { return 0; }
bool FirstPersonShadowPass() { return false; }
const float* FirstPersonHandSkin(int&) { return nullptr; }
const float* FirstPersonBodySkin(uint32_t&) { return nullptr; }
uint32_t FirstPersonBodyWristSeamMask() { return 0; }
const float* FirstPersonShadowSkin(uint32_t&,uint32_t&) { return nullptr; }
bool FirstPersonWristCentre(float*) { return false; }
bool EnhancedEffectsReady() { return false; }
void EnhancedEffectsInvalidateProgram(unsigned) {}
#else
bool FirstPersonNativeMirrorDrawing() { return false; }
const float* FirstPersonBodyPalette(uint64_t&,int&) { return nullptr; }
const float* FirstPersonSealedHandSkin(int&,float*) { return nullptr; }
bool FirstPersonKeypadLight(float*) { return false; }
bool MirrorReflectionCapturing() { return false; }
void MirrorReflectionObserver(float*) {}
const float* FirstPersonRenderBodyPalette(uint64_t&,int&) { return nullptr; }
uint64_t FirstPersonBodyWristSeamMask() { return 0; }
int FirstPersonTrackedHandJoint() { return -1; }
#endif
firstperson::TorsoSurface sample;
int model;
unsigned submissions=0;
bool FirstPersonTorsoMeasureInfo(float inverse[12],uint64_t& mask,bool& measured) {
    const float m[12]={0,0,-1,10,0,2,0,20,1,0,0,30};
    std::memcpy(inverse,m,sizeof(m));mask=1ull<<9;measured=sample.valid;return true;
}
bool FirstPersonTorsoBufferKnown(unsigned buffer) { return sample.Known(buffer); }
void FirstPersonTorsoMeasure(unsigned buffer,const float a[3],const float b[3],unsigned count) {
    ++submissions;sample.Add(&model,buffer,{a[0],a[1],a[2]},{b[0],b[1],b[2]},count);
}
}
namespace hook {
InlineHook::~InlineHook() {}
void InlineHook::Remove() {}
bool InlineHook::Install(void*,void*,size_t,const uint8_t*,size_t,const char*,const int*,size_t) { return false; }
}
GLuint CompileShader(GLenum type,const char* text) {
    GLuint s=gl::CreateShader(type);gl::ShaderSource(s,1,&text,nullptr);gl::CompileShader(s);
    GLint ok=0;gl::GetShaderiv(s,GL_COMPILE_STATUS,&ok);Check(ok!=0,"fixture shader compiles");return s;
}
int main() {
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MotionHandTest";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Hand test",WS_OVERLAPPED,0,0,256,256,nullptr,nullptr,wc.hInstance,nullptr);
    HDC dc=GetDC(window);PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;
    Check(SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)!=0,"set pixel format");
    HGLRC legacy=wglCreateContext(dc);Check(legacy && wglMakeCurrent(dc,legacy),"create GL context");
    using Create=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    auto create=reinterpret_cast<Create>(wglGetProcAddress("wglCreateContextAttribsARB"));Check(create!=nullptr,"core context available");
    const int attribs[]={0x2091,3,0x2092,2,0x9126,1,0};HGLRC core=create(dc,nullptr,attribs);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);Check(core && wglMakeCurrent(dc,core),"activate GL 3.2");
    Check(gl::Load() && gl::LoadedSkinApi(),"load skin API");
    std::printf("GL: %s\n",glGetString(GL_VERSION));


    using namespace tr;
    testConfig.dynamicBonesForwardSign=0;
    testConfig.dynamicBonesShader=1;testConfig.dynamicBonesApply=true;
    testConfig.dynamicBonesChestStrength=2.5f;testConfig.dynamicBonesDebugScale=1;
    testConfig.dynamicBonesStiffness=630;testConfig.dynamicBonesGravity=5400;
    testDll.laraItem=8;const auto itemAddress=reinterpret_cast<uint64_t>(itemMemory);
    std::memcpy(gameMemory+8,&itemAddress,sizeof(itemAddress));
    g_boundDll=&testDll;g_settled=true;
    for(float yaw:{0.f,.7f,2.4f,3.1415926f,1.5707963f,-2.4f}) {
        auto& root=g_best.joints[0];root={};
        root.m[0]=root.m[10]=std::cos(yaw);root.m[2]=std::sin(yaw);root.m[8]=-std::sin(yaw);root.m[5]=1;
        g_torsoThis=root;
        g_fwd=0;g_fwdSamples=0;g_fwdEvidence=0;
        for(int frame=0;frame<30;++frame) UpdateForward();
        Check(g_fwd==1,"chest front selection cannot reverse with the palette/render heading");
    }
    g_best.joints[0]={};g_torsoThis={};
    g_torsoThis.m[0]=g_torsoThis.m[5]=g_torsoThis.m[10]=1;
    g_best.joints[0]=g_torsoThis;
    g_fwd=1;g_regionReady=true;g_bodyCalls=1;
    g_region={{-200,-100,5,7,1,0,50,0,0,30,50,0}};
    g_drawRenderBody=true;g_patched=1;
    std::string vertex=R"GLSL(#version 150
in vec3 aCoord;in vec4 aLight;in vec4 aColor;
uniform vec4 uJoints[45];
void main(){
vec4 coord = vec4(aCoord, 1.0);
vec4 j = aLight;
vec4 w = aColor;
ivec3 index = ivec3(j.xyz);index*=3;
vec4 p=vec4(0,0,0,1);
float weight=w.x;
p.x=dot(uJoints[index[0]],coord)*weight;
p.y=dot(uJoints[index[0]+1],coord)*weight;
p.z=dot(uJoints[index[0]+2],coord)*weight;
weight=w.y;
p.x+=dot(uJoints[index[1]],coord)*weight;
p.y+=dot(uJoints[index[1]+1],coord)*weight;
p.z+=dot(uJoints[index[1]+2],coord)*weight;
weight=w.z;
p.x+=dot(uJoints[index[2]],coord)*weight;
p.y+=dot(uJoints[index[2]+1],coord)*weight;
p.z += dot(uJoints[index[2] + 2], coord) * weight;
gl_Position=vec4(p.x/500.,p.y/500.,0,1);
})GLSL";
    Check(Recognise(vertex),"production chest patch recognises the skinned GPU fixture");
    vertex=Patch(vertex);
    std::string fragment="#version 150\nout vec4 color;void main(){color=vec4(1);}";
#ifdef TR123_TORSO_TEST
    Check(handskin::Patch(vertex,fragment),"body visibility composes with chest motion");
#else
    Check(motionhandskin::Patch(vertex,fragment),"body visibility composes with chest motion");
#endif
    GLuint vs=CompileShader(GL_VERTEX_SHADER,vertex.c_str()),fs=CompileShader(GL_FRAGMENT_SHADER,fragment.c_str());
    GLuint program=gl::CreateProgram();gl::AttachShader(program,vs);gl::AttachShader(program,fs);gl::LinkProgram(program);
    GLint linked=0;gl::GetProgramiv(program,GL_LINK_STATUS,&linked);Check(linked!=0,"chest GPU fixture links");gl::UseProgram(program);
    Shaders()[0].id=program;VidState().shader=0;
    float palette[180]{};
    for(int joint=0;joint<15;++joint) palette[joint*12]=palette[joint*12+5]=palette[joint*12+10]=.85f;
    gl::Uniform4fv(gl::GetUniformLocation(program,"uJoints"),45,palette);
    GLuint vao=0,vbo=0;gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);gl::GenBuffers(1,&vbo);gl::BindBuffer(GL_ARRAY_BUFFER,vbo);
    struct Vertex{float p[3],j[4],w[4];};Vertex point{{0,-150,50},{7,0,0,0},{1,0,0,0}};
    const char* names[]={"aCoord","aLight","aColor"};
    for(int i=0;i<3;++i){auto loc=gl::GetAttribLocation(program,names[i]);gl::EnableVertexAttribArray(loc);gl::VertexAttribPointer(loc,i?4:3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i==0?0:i==1?12:28)));}
    glViewport(0,0,128,128);glDisable(GL_DEPTH_TEST);glPointSize(5);
    auto renderedY=[&](float z,float offset) {
        point.p[2]=z;gl::BufferData(GL_ARRAY_BUFFER,sizeof(point),&point,GL_STATIC_DRAW);
        for(auto& bone:g_bone){bone.x[0]=bone.x[2]=0;bone.x[1]=testConfig.dynamicBonesGravity/testConfig.dynamicBonesStiffness+offset;}
#ifdef TR123_TORSO_TEST
        BoneSkinAfterValidate(false);
#else
        BoneSkinAfterValidate();
#endif
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_POINTS,0,1);
        unsigned char pixels[128*128*4]{};glReadPixels(0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        float sum=0;int count=0;for(int y=0;y<128;++y)for(int x=0;x<128;++x)if(pixels[(y*128+x)*4]>128){sum+=y;++count;}
        Check(count>0,"fitted body vertex remains visible on the GPU");return sum/count;
    };
    const float front=renderedY(50,0),frontBounce=renderedY(50,20);
    const float back=renderedY(-50,0),backBounce=renderedY(-50,20);
    Check(frontBounce-front>5,"live spring displacement visibly moves the scaled front chest on GPU");
    Check(std::fabs(backBounce-back)<.01f,"chest bounce leaves the back vertices still");
    Check(glGetError()==GL_NO_ERROR,"chest render leaves no GL errors");
    std::printf("PASS: %d chest direction and GPU deformation checks\n",checks);
}
