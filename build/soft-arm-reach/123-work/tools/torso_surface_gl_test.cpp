// Exercise the production GL buffer reader on a real driver, without a game.
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
uint64_t GameDllBase() { return 0; }
const Config& Cfg() { static Config c;return c; }
RenderState& VidState() { static RenderState s;return s; }
Shader* Shaders() { static Shader s[202];return s; }
bool FirstPersonBodyClearance(float*,uint64_t&) { return false; }
bool DynamicBonesRenderBody() { return false; }
bool DynamicBonesWorldOffset(float*) { return false; }
bool DynamicBonesTorsoFrame(float*) { return false; }
bool DynamicBonesLaraForward(float*) { return false; }
#ifdef TR123_TORSO_TEST
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
GLuint Shader(GLenum type,const char* text) {
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

    GLuint vs=Shader(GL_VERTEX_SHADER,"#version 150\nin vec3 aCoord;in vec4 aLight;in vec4 aColor;void main(){gl_Position=vec4(aCoord+aLight.xyz*.0001+aColor.xyz*.0001,1);}");
    GLuint fs=Shader(GL_FRAGMENT_SHADER,"#version 150\nout vec4 color;void main(){color=vec4(1);}");
    GLuint program=gl::CreateProgram();gl::AttachShader(program,vs);gl::AttachShader(program,fs);gl::LinkProgram(program);
    GLint linked=0;gl::GetProgramiv(program,GL_LINK_STATUS,&linked);Check(linked!=0,"fixture shader links");gl::UseProgram(program);
    GLuint vao=0,vbo=0,sentinel=0;gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);
    gl::GenBuffers(1,&vbo);gl::GenBuffers(1,&sentinel);gl::BindBuffer(GL_ARRAY_BUFFER,vbo);
    struct Vertex {float xyz[3];uint8_t joints[4],weights[4];float padding;};
    Vertex vertices[16]{};
    for(int i=0;i<16;++i) {
        auto& v=vertices[i];v.xyz[0]=(i&1)?2.f:-2.f;v.xyz[1]=(i&2)?3.f:-3.f;v.xyz[2]=(i&4)?4.f:-4.f;
        v.joints[0]=i<8?9:2;v.joints[1]=2;v.weights[0]=128;v.weights[1]=127;
        if(i>=8) for(float& p:v.xyz) p*=1000;
    }
    gl::BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    const char* names[]={"aCoord","aLight","aColor"};
    for(int i=0;i<3;++i) {
        const GLuint attr=GLuint(gl::GetAttribLocation(program,names[i]));gl::EnableVertexAttribArray(attr);
        gl::VertexAttribPointer(attr,i?4:3,i?GL_UNSIGNED_BYTE:GL_FLOAT,i==2?GL_TRUE:GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i==0?0:i==1?12:16)));
    }
    gl::BindBuffer(GL_ARRAY_BUFFER,sentinel);
    tr::MeasureFirstPersonTorso(program);
    Check(tr::sample.valid,"production reader captures byte-packed weighted torso vertices");
    Check(tr::sample.low.x==6 && tr::sample.high.x==14 && tr::sample.low.y==14 && tr::sample.high.y==26 &&
          tr::sample.low.z==28 && tr::sample.high.z==32,"measured bounds use inverse chest bind and exclude distant arm vertices");
    GLint buffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);Check(GLuint(buffer)==sentinel,"measurement restores the caller's array buffer");
    Check(tr::submissions==1,"first draw measures the body once");
    for(int i=0;i<240;++i) tr::MeasureFirstPersonTorso(program);
    Check(tr::submissions==1,"repeated stereo and material draws reuse measured geometry");
    tr::sample.Reset();tr::MeasureFirstPersonTorso(program);
    Check(tr::submissions==2 && tr::sample.valid,"recenter or outfit reset permits a fresh mesh measurement");
    Check(glGetError()==GL_NO_ERROR,"buffer measurement leaves no GL error");
    std::printf("PASS: %d torso surface GL checks\n",checks);
}
