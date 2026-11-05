// Hidden-window real-driver regression. Never loads or executes game DLLs.
#include "../src/BoneSkin.cpp"
#include "../build/keypad-shaders.h"
#include <cstdarg>
#include <cstdlib>
void Log(const char* s) { std::puts(s); }
void LogF(const char* f,...) { va_list a;va_start(a,f);std::vprintf(f,a);va_end(a);std::puts(""); }
int checks=0;
void Check(bool ok,const char* label) { ++checks;if(!ok) {std::printf("FAIL: %s\n",label);std::exit(1);} }
GLuint Compile(GLenum kind,const std::string& source) {
    const char* p=source.c_str();GLuint s=gl::CreateShader(kind);gl::ShaderSource(s,1,&p,nullptr);gl::CompileShader(s);
    GLint ok=0;gl::GetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]{};gl::GetShaderInfoLog(s,sizeof(log)-1,nullptr,log);std::puts(log); }
    Check(ok!=0,"shader compiles");return s;
}
GLuint Link(GLuint vs,GLuint fs) {
    GLuint p=gl::CreateProgram();gl::AttachShader(p,vs);gl::AttachShader(p,fs);gl::LinkProgram(p);
    GLint ok=0;gl::GetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok) {gl::DeleteProgram(p);return 0;}return p;
}
namespace tr {
float requestedLight[4]{};
uint64_t Base() { return 0; }
const Layout& L() { static Layout l;return l; }
bool IsWorldPass() { return true; }
uint64_t GameDllBase() { return 0; }
const float* FirstPersonSealedHandSkin(int&,float*) { return nullptr; }
const Config& Cfg() { static Config c;return c; }
RenderState& VidState() { static RenderState s;return s; }
Shader* Shaders() { static Shader s[202];return s; }
bool FirstPersonKeypadLight(float out[4]) { std::memcpy(out,requestedLight,sizeof(requestedLight));return out[2]>0; }
bool MirrorReflectionCapturing() { return false; }
void MirrorReflectionObserver(float out[4]) { std::memset(out,0,16); }
const float* FirstPersonRenderBodyPalette(uint64_t&,int&) { return nullptr; }
uint64_t FirstPersonBodyWristSeamMask() { return 0; }
int FirstPersonTrackedHandJoint() { return -1; }
bool DynamicBonesRenderBody() { return false; }
bool DynamicBonesWorldOffset(float*) { return false; }
bool DynamicBonesTorsoFrame(float*) { return false; }
bool DynamicBonesLaraForward(float*) { return false; }
}
namespace hook {
InlineHook::~InlineHook() {}
void InlineHook::Remove() {}
bool InlineHook::Install(void*,void*,size_t,const uint8_t*,size_t,const char*,const int*,size_t) { return false; }
}
int main() {
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"KeypadLightTest";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Keypad light test",WS_OVERLAPPED,0,0,256,256,nullptr,nullptr,wc.hInstance,nullptr);
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
    std::string v=NativeKeypadVertex,f=NativeKeypadFragment;
    Check(motionhandskin::Patch(v,f),"hand visibility patch accepts shipped keypad shader");
    Check(keypad::PatchLight(v,f),"light patch composes with hand patch on actual keypad shader 9");
    const auto onceV=v,onceF=f;
    Check(!keypad::PatchLight(v,f) && v==onceV && f==onceF,"duplicate light patch leaves shader unchanged");
    GLuint programs[3]{};
    for(int p=0;p<3;++p) {
        const GLuint vs=Compile(GL_VERTEX_SHADER,p ? v : NativeKeypadVertex),fs=Compile(GL_FRAGMENT_SHADER,p ? f : NativeKeypadFragment);
        programs[p]=Link(vs,fs);Check(programs[p]!=0,"shipped keypad shader links");
        gl::DeleteShader(vs);gl::DeleteShader(fs);
    }
    GLuint target=0,fbo=0,vao=0,vbo=0;glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl::GenFramebuffers(1,&fbo);gl::BindFramebuffer(GL_FRAMEBUFFER,fbo);gl::FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    Check(gl::CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"key light pixel target complete");
    gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);gl::GenBuffers(1,&vbo);gl::BindBuffer(GL_ARRAY_BUFFER,vbo);
    glViewport(0,0,128,128);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    using Image3D=void(APIENTRY*)(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
    auto image3D=reinterpret_cast<Image3D>(wglGetProcAddress("glTexImage3D"));Check(image3D!=nullptr,"array texture upload");
    GLuint textures[2]{};glGenTextures(2,textures);
    gl::ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,textures[0]);
    const unsigned char texels[]={64,64,64,128,204,204,204,128};
    image3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,2,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    gl::ActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D_ARRAY,textures[1]);
    const float depth=1;image3D(GL_TEXTURE_2D_ARRAY,0,GL_DEPTH_COMPONENT24,1,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,0x884C,0x884E);glTexParameteri(GL_TEXTURE_2D_ARRAY,0x884D,GL_LEQUAL);
    auto render=[&](int p,int joint,int key,float strength) {
        const GLuint program=programs[p];gl::UseProgram(program);
        auto uniform=[&](const char* name,const float* data,int count=1) { gl::Uniform4fv(gl::GetUniformLocation(program,name),count,data); };
        const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},white[]={1,1,1,1};
        gl::UniformMatrix4fv(gl::GetUniformLocation(program,"uProjMatrix"),1,GL_FALSE,identity);
        gl::UniformMatrix4fv(gl::GetUniformLocation(program,"uShadowMatrix"),1,GL_FALSE,identity);
        uniform("uViewMatrix",identity,4);uniform("uColor",white);uniform("uFogPlane",white);
        float palette[72*12]{};for(int j=0;j<72;++j) palette[j*12]=palette[j*12+5]=palette[j*12+10]=1;
        uniform("uJoints",palette,72*3);
        float ambient[24]{};for(int i=0;i<6;++i) for(int c=0;c<3;++c) ambient[i*4+c]=.04f;
        uniform("uAmbient",ambient,6);
        float lights[32]{};for(int i=0;i<8;++i) {lights[i*4+1]=100;lights[i*4+3]=1;}
        uniform("uLightPos",lights,8);
        gl::Uniform1i(gl::GetUniformLocation(program,"sShadow"),1);gl::Uniform1i(gl::GetUniformLocation(program,"sNoise"),2);
        const float xy[6][2]={{-1,-1},{1,-1},{-1,1},{1,-1},{1,1},{-1,1}};
        float data[6][15]{};
        for(int i=0;i<6;++i) {
            data[i][0]=xy[i][0];data[i][1]=xy[i][1];data[i][2]=.5f;
            data[i][3]=127;data[i][4]=127;data[i][5]=254;
            data[i][7]=float(joint);data[i][10]=(xy[i][0]+1)*127.5f;
            data[i][11]=1;data[i][14]=(xy[i][1]+1)*.5f;
        }
        gl::BufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STATIC_DRAW);
        const char* names[]={"aCoord","aNormal","aLight","aColor"};const int sizes[]={3,4,4,4},offsets[]={0,3,7,11};
        for(int i=0;i<4;++i) {
            const int loc=gl::GetAttribLocation(program,names[i]);Check(loc>=0,"native keypad vertex input present");
            gl::EnableVertexAttribArray(loc);gl::VertexAttribPointer(loc,sizes[i],GL_FLOAT,GL_FALSE,15*sizeof(float),reinterpret_cast<void*>(size_t(offsets[i])*4));
        }
        requestedLight[0]=float(key+1);requestedLight[1]=float(key+13);requestedLight[2]=strength;
        if(p) {VidState().shader=9;Shaders()[9].id=program;BoneSkinAfterValidate();}
        glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);
        std::vector<unsigned char> pixels(8);glReadPixels(32,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glReadPixels(96,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()+4);
        return pixels;
    };
    const auto baseline=render(0,1,0,0);
    Check(baseline[1]<25 && baseline[5]>baseline[1],"native dark-room shader retains textured key contrast");
    for(int key=0;key<12;++key) for(int joint=0;joint<25;++joint) {
        const bool lit=joint==key+1 || joint==key+13;
        const auto pixels=render(1,joint,key,1);
        if(lit) {
            Check(pixels[1]>baseline[1]+45 && pixels[5]>baseline[5]+150,"each raised/depressed key becomes visibly bright");
            Check(pixels[5]-pixels[1]>100 && pixels[3]==baseline[3] && pixels[7]==baseline[7],"lit key preserves lettering contrast and alpha");
        } else Check(pixels==baseline,"panel frame and every untouched key remain pixel-identical");
    }
    const auto full=render(1,1,0,1),fading=render(1,1,0,.5f);
    Check(fading[5]>baseline[5] && fading[5]<full[5],"fading key brightness stays between lit and native");
    render(2,1,0,1);render(1,1,0,1);
    Check(render(2,1,0,0)==baseline && render(1,1,0,0)==baseline,"production uniform reset clears every reused shader program");
    Check(glGetError()==GL_NO_ERROR,"key light renders without GL errors");
    gl::UseProgram(0);gl::BindVertexArray(0);glDeleteTextures(2,textures);glDeleteTextures(1,&target);
    gl::DeleteFramebuffers(1,&fbo);gl::DeleteBuffers(1,&vbo);gl::DeleteVertexArrays(1,&vao);
    for(auto p:programs) gl::DeleteProgram(p);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(core);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("PASS: %d keypad real-GL checks\n",checks);return 0;
}
