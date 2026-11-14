#include "RenderAuditReadback.h"
#include <cstdarg>
#include <cstdlib>
#include <cstring>
void Log(const char* s) { std::puts(s); }
void LogF(const char* f,...) { va_list a;va_start(a,f);std::vprintf(f,a);va_end(a);std::puts(""); }
int checks=0;
void Check(bool ok,const char* label) { ++checks;if(!ok) { std::printf("FAIL: %s\n",label);std::exit(1); } }
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


    const char* source="#version 150\nuniform mat4 uProjMatrix;uniform vec4 uViewMatrix[3];uniform vec4 uJoints[24];void main(){int i=(gl_VertexID%8)*3;vec3 p=vec3(uJoints[i].w,uJoints[i+1].w,uJoints[i+2].w);gl_Position=uProjMatrix*vec4(dot(uViewMatrix[0],vec4(p,1)),dot(uViewMatrix[1],vec4(p,1)),dot(uViewMatrix[2],vec4(p,1)),1);}";
    GLuint vs=Shader(GL_VERTEX_SHADER,source);
    GLuint fs=Shader(GL_FRAGMENT_SHADER,"#version 150\nout vec4 color;void main(){color=vec4(1);}");
    GLuint program=gl::CreateProgram();gl::AttachShader(program,vs);gl::AttachShader(program,fs);gl::LinkProgram(program);
    GLint linked=0;gl::GetProgramiv(program,GL_LINK_STATUS,&linked);Check(linked!=0,"audit fixture links");gl::UseProgram(program);
    float projection[16],view[12],skin[96];
    for(int i=0;i<16;++i) projection[i]=float(i)*.125f;
    for(int i=0;i<12;++i) view[i]=float(i)*.25f;
    for(int i=0;i<96;++i) skin[i]=float(i)*.5f;
    gl::UniformMatrix4fv(gl::GetUniformLocation(program,"uProjMatrix"),1,GL_FALSE,projection);
    gl::Uniform4fv(gl::GetUniformLocation(program,"uViewMatrix[0]"),3,view);
    gl::Uniform4fv(gl::GetUniformLocation(program,"uJoints[0]"),24,skin);
    gl::UseProgram(0);
    tr::renderaudit::Draw result{};
    Check(tr::renderaudit::Read(program,true,result),"reads live final uniforms without binding the sampled program");
    Check(std::memcmp(result.projection,projection,sizeof(projection))==0,"full projection includes off-axis and translation terms");
    Check(std::memcmp(result.view,view,sizeof(view))==0,"reads every view row with its own uniform location");
    Check(std::memcmp(result.skin,skin,12*sizeof(float))==0 && std::memcmp(result.skin+12,skin+84,12*sizeof(float))==0,"reads distinct pelvis and chest palette slots");
    GLint bound=-1;glGetIntegerv(GL_CURRENT_PROGRAM,&bound);Check(bound==0,"audit leaves program binding untouched");
    tr::renderaudit::Draw again{};Check(tr::renderaudit::Read(program,true,again),"repeated read succeeds");
    Check(std::memcmp(&result,&again,sizeof(result))==0,"audit does not mutate uniforms");
    Check(tr::renderaudit::Read(program,false,again) && !again.skinned,"scenery capture skips skin palette");
    again.view[0]=123;Check(!tr::renderaudit::Read(0,true,again) && again.view[0]==123,"invalid program fails without altering output");
    Check(glGetError()==GL_NO_ERROR,"readbacks leave no GL error");
    std::printf("PASS: %d render audit GL checks\n",checks);
}
