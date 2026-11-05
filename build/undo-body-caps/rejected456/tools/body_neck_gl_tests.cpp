// Real GL pixel regressions for the production shader patch; no headset needed.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdarg>
#include <cstdint>
#include "../src/GL.h"
#include "../src/MotionHandSkin.h"
#include "../src/BodyClearanceShader.h"
#include "../src/BodyNeckCap.h"
#include "../src/BodyClearanceCap.h"
#include "../src/SurfaceCapShader.h"
#ifdef TR123_CLEARANCE_TEST
#include "HandSkin.h"
#endif
bool PatchClearance(std::string& v,std::string& f) {
#ifdef TR123_CLEARANCE_TEST
    return tr::handskin::Patch(v,f) && tr::bodyclearance::Patch(v,f);
#else
    return tr::motionhandskin::Patch(v,f) && tr::bodyclearance::Patch(v,f);
#endif
}

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
std::vector<std::string> Sources(const char* filename) {
    std::ifstream file(filename,std::ios::binary);Check(bool(file),"engine binary opens read-only");
    std::string bytes((std::istreambuf_iterator<char>(file)),{});std::vector<std::string> out;
    size_t at=0;
    while((at=bytes.find("#version 150",at))!=std::string::npos) {
        auto end=bytes.find('\0',at);if(end==std::string::npos) break;
        out.push_back(bytes.substr(at,end-at));at=end+1;
    }
    return out;
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

    using namespace tr;using namespace motiongun;
    GLuint vs=Compile(GL_VERTEX_SHADER,surfacecap::Vertex),fs=Compile(GL_FRAGMENT_SHADER,surfacecap::Fragment);
    GLuint program=Link(vs,fs);Check(program!=0,"production surface cap shaders link");
    GLuint tex=0,fbo=0,vao=0,vbo=0;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl::GenFramebuffers(1,&fbo);gl::BindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    Check(gl::CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"offscreen target complete");
    gl::UseProgram(program);gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);
    gl::GenBuffers(1,&vbo);gl::BindBuffer(GL_ARRAY_BUFFER,vbo);
    const GLint pos=gl::GetAttribLocation(program,"aPosition");
    gl::VertexAttribPointer(pos,3,GL_FLOAT,GL_FALSE,sizeof(Vec),nullptr);gl::EnableVertexAttribArray(pos);
    float projection[16]{};projection[0]=projection[5]=1.f/160;projection[10]=1.f/1000;projection[15]=1;
    const float view[12]={1,0,0,0,0,0,1,0,0,1,0,0};
    gl::UniformMatrix4fv(gl::GetUniformLocation(program,"uProjection"),1,GL_FALSE,projection);
    gl::Uniform4fv(gl::GetUniformLocation(program,"uView[0]"),3,view);
    glViewport(0,0,128,128);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    int cases=0;
    for(int head:{14,31,32}) for(float shift:{-25.f,0.f,25.f}) {
        bodyneckcap::Builder builder;
        auto vertex=[&](int slice,bool upper) {
            const float angle=float(slice)*6.28318530718f/16;
            wristcap::Vertex v{};v.position={60*std::cos(angle),upper?100.f:300.f,60*std::sin(angle)};
            v.joint[0]=upper?head:7;v.weight[0]=1;return v;
        };
        for(int i=0;i<16;++i) {
            wristcap::Vertex a[3]={vertex(i,false),vertex(i+1,false),vertex(i+1,true)};
            wristcap::Vertex b[3]={vertex(i,false),vertex(i+1,true),vertex(i,true)};
            builder.AddTriangle(a,uint64_t(1)<<head);builder.AddTriangle(b,uint64_t(1)<<head);
            // Duplicate material triangles must not invent or remove openings.
            builder.AddTriangle(a,uint64_t(1)<<head);
        }
        const auto loops=builder.Finish((uint64_t(1)<<head)|(uint64_t(1)<<7));
        Check(loops.size()==2,"closed neck cut and pre-existing lower boundary recovered across materials");
        Check(builder.Finish(uint64_t(1)<<head).empty(),"non-body attachment boundary is rejected");
        float palette[33*12]{};
        for(int j=0;j<33;++j) {palette[j*12]=palette[j*12+5]=palette[j*12+10]=1;}
        palette[head*12+3]=shift;palette[head*12+7]=-20;
        for(float stereo:{-32.f,32.f}) {
            for(int j=0;j<33;++j) palette[j*12+3]-=stereo;
            const auto triangles=bodyneckcap::Triangles(loops,palette,33,{-stereo,0,0},1000);
            Check(!triangles.empty(),"nearest neck opening produces a cap");
            for(auto p:triangles) Check(std::fabs(p.y-190)<.01f,"rim exactly matches interpolated skinned cutoff");
            Check(std::fabs(triangles[0].x-(shift*.5f-stereo))<.01f,"cap shares body deformation and stereo camera frame");
            Check(bodyneckcap::Triangles(loops,palette,33,{-stereo,500,0},1000).empty(),"opening above camera cannot cover the view");
            glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            gl::BufferData(GL_ARRAY_BUFFER,triangles.size()*sizeof(Vec),triangles.data(),GL_STATIC_DRAW);
            glDrawArrays(GL_TRIANGLES,0,GLsizei(triangles.size()));
            unsigned char centre[4]{},outside[4]{};
            glReadPixels(64,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,centre);
            glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,outside);
            Check(centre[0]==0 && centre[1]==0 && centre[2]==0 && centre[3]==255,"look-down ray hits an opaque neck seal");
            Check(outside[0]==255 && outside[3]==255,"seal does not cover the surrounding view");
            for(int j=0;j<33;++j) palette[j*12+3]+=stereo;
            ++cases;
        }
    }

    // Reproduce the reported chest case: the camera is inside a torso tube,
    // with visible body extending above and below the eye. The clearance cut
    // must be filled, including after translating the camera for either eye.
    for(float stereo:{-32.f,32.f}) {
        std::vector<bodyclearancecap::Triangle> mesh;
        auto v=[&](int i,float y) {
            wristcap::Vertex p{};const float a=i*6.28318530718f/16;
            p.position={60*std::cos(a),y,60*std::sin(a)};p.joint[0]=7;p.weight[0]=1;return p;
        };
        for(int i=0;i<16;++i) {
            mesh.push_back({v(i,-100),v(i+1,-100),v(i+1,300)});
            mesh.push_back({v(i,-100),v(i+1,300),v(i,300)});
        }
        float palette[33*12]{};
        for(int j=0;j<33;++j) {palette[j*12]=palette[j*12+5]=palette[j*12+10]=1;palette[j*12+3]=-stereo;}
        auto triangles=bodyclearancecap::Triangles(mesh,palette,33,1ull<<7,1ull<<7,{-stereo,0,0},200);
        Check(!triangles.empty(),"camera inside torso produces a closed clearance cap");
        bodyneckcap::Deformation deformation{};
        deformation.bone[0]=17;deformation.bone[2]=12;deformation.bone[3]=1;
        const float region[12]={-1000,1000,1,7,1,-1000,1,0,0,1000,1100,0};
        std::memcpy(deformation.region,region,sizeof(region));
        const auto animated=bodyclearancecap::Triangles(mesh,palette,33,1ull<<7,1ull<<7,{-stereo,0,0},200,deformation);
        Check(!animated.empty() && std::fabs(animated[0].x+stereo-17)<.01f && std::fabs(animated[0].z-12)<.01f,
              "chest cap follows the same per-vertex dynamic chest offset as the body shader");

        for(auto p:triangles) Check(std::fabs(p.y-200)<.001f,"chest cap precisely matches shader trim plane");
        Check(bodyclearancecap::Triangles(mesh,palette,33,1ull<<14,1ull<<7,{-stereo,0,0},200).empty(),"tracked arms and non-body surfaces cannot produce chest caps");
        Check(bodyclearancecap::Triangles(mesh,palette,33,1ull<<7,0,{-stereo,0,0},200).empty(),"hidden body cannot leave a floating cap");
        glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        gl::BufferData(GL_ARRAY_BUFFER,triangles.size()*sizeof(Vec),triangles.data(),GL_STATIC_DRAW);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(triangles.size()));
        unsigned char pixel[4]{};glReadPixels(64,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        Check(pixel[0]==0 && pixel[3]==255,"looking down from inside chest sees an opaque surface instead of interior");
        ++cases;
    }
    Check(glGetError()==GL_NO_ERROR,"cap rendering has no GL errors");
    gl::UseProgram(0);gl::BindVertexArray(0);gl::DeleteBuffers(1,&vbo);gl::DeleteVertexArrays(1,&vao);
    gl::BindFramebuffer(GL_FRAMEBUFFER,0);gl::DeleteFramebuffers(1,&fbo);glDeleteTextures(1,&tex);
    gl::DeleteProgram(program);gl::DeleteShader(vs);gl::DeleteShader(fs);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(core);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("PASS: %d body neck seal checks across %d GL renders.\n",checks,cases);
}
