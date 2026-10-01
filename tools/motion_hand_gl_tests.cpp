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
#include "../src/GL.h"
#include "../src/MotionHandSkin.h"

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
int main(int argc,char** argv) {
    Check(argc==2,"pass path to installed tomb456.exe");
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
    const auto sources=Sources(argv[1]);
    std::vector<std::string> vertices,fragments;
    for(const auto& s:sources) {
        if(s.find("p.z += dot(uJoints[index[2] + 2], coord) * weight;")!=std::string::npos) vertices.push_back(s);
        else if(s.find("gl_Position")==std::string::npos && s.find("void main()")!=std::string::npos) fragments.push_back(s);
    }
    Check(vertices.size()==5,"all five shipped weighted skin shader variants found");
    int pairs=0;
    for(const auto& vertex:vertices) {
        GLuint originalVs=Compile(GL_VERTEX_SHADER,vertex);int perVertex=0;
        for(const auto& fragment:fragments) {
            GLuint originalFs=Compile(GL_FRAGMENT_SHADER,fragment);
            GLuint original=Link(originalVs,originalFs);gl::DeleteShader(originalFs);
            if(!original) continue;
            gl::DeleteProgram(original);
            auto v=vertex,f=fragment;Check(tr::motionhandskin::Patch(v,f),"production pair recognized");
            GLuint vs=Compile(GL_VERTEX_SHADER,v),fs=Compile(GL_FRAGMENT_SHADER,f),p=Link(vs,fs);
            Check(p!=0,"patched shipped shader pair links");gl::DeleteProgram(p);gl::DeleteShader(vs);gl::DeleteShader(fs);++pairs;++perVertex;
        }
        Check(perVertex>0,"each shipped vertex variant has a tested linked fragment");gl::DeleteShader(originalVs);
    }
    std::printf("Verified %d compatible shipped shader pairs.\n",pairs);
    // Pixel fixture uses the real native shader, with only its final projection
    // replaced by identity so positions are directly measurable in the FBO.
    std::string vertex=vertices[0];const auto pos=vertex.find("gl_Position =");
    Check(pos!=std::string::npos,"native projection assignment found");
    vertex.replace(pos,vertex.find(';',pos)-pos+1,"gl_Position = p;");
    const std::string fragment="#version 150\nout vec4 color;void main(){color=vec4(0,1,0,1);}";
    auto v=vertex,f=fragment;Check(tr::motionhandskin::Patch(v,f),"pixel fixture patched");
    const auto onceV=v,onceF=f;Check(!tr::motionhandskin::Patch(v,f) && v==onceV && f==onceF,"repeat patch rejected without source mutation");
    GLuint nativeVs=Compile(GL_VERTEX_SHADER,vertex),nativeFs=Compile(GL_FRAGMENT_SHADER,fragment);
    GLuint fixedVs=Compile(GL_VERTEX_SHADER,v),fixedFs=Compile(GL_FRAGMENT_SHADER,f);
    GLuint native=Link(nativeVs,nativeFs),fixed=Link(fixedVs,fixedFs);Check(native && fixed,"pixel programs link");
    GLuint tex=0,fbo=0,vao=0,vbo=0;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl::GenFramebuffers(1,&fbo);gl::BindFramebuffer(GL_FRAMEBUFFER,fbo);gl::FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    Check(gl::CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"pixel target complete");
    gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);gl::GenBuffers(1,&vbo);gl::BindBuffer(GL_ARRAY_BUFFER,vbo);
    glViewport(0,0,128,128);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    auto render=[&](GLuint program,int hand,float weight,bool enabled,float angle) {
        gl::UseProgram(program);const GLint uniform=gl::GetUniformLocation(program,"uTrackedHandJoint");
        if(uniform>=0) gl::Uniform1i(uniform,enabled ? hand+1 : 0);
        float palette[72*12]{};float* wrist=palette+hand*12;
        wrist[0]=wrist[5]=std::cos(angle);wrist[1]=-std::sin(angle);wrist[4]=std::sin(angle);wrist[10]=1;
        gl::Uniform4fv(gl::GetUniformLocation(program,"uJoints"),72*3,palette);
        const float xy[6][2]={{.3f,-.2f},{.8f,-.2f},{.3f,.2f},{.8f,-.2f},{.8f,.2f},{.3f,.2f}};
        float data[6][15]{};
        for(int i=0;i<6;++i) {
            data[i][0]=xy[i][0];data[i][1]=xy[i][1];data[i][3]=127;data[i][4]=127;data[i][5]=254;
            data[i][7]=float(hand);data[i][8]=float(hand-1);data[i][11]=weight;data[i][12]=1-weight;
        }
        gl::BufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STATIC_DRAW);
        const char* names[]={"aCoord","aNormal","aLight","aColor"};const int sizes[]={3,4,4,4},offsets[]={0,3,7,11};
        for(int i=0;i<4;++i) { GLint a=gl::GetAttribLocation(program,names[i]);if(a>=0) {gl::EnableVertexAttribArray(a);gl::VertexAttribPointer(a,sizes[i],GL_FLOAT,GL_FALSE,sizeof(data[0]),reinterpret_cast<void*>(size_t(offsets[i]*4)));} }
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);
        std::vector<unsigned char> pixels(128*128*4);glReadPixels(0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
    };
    for(int hand:{10,13}) for(float angle:{0.f,1.2f,-1.4f}) {
        const auto rigid=render(native,hand,1,false,angle);
        const auto warped=render(native,hand,.75f,false,angle);
        Check(warped!=rigid,"native hidden-forearm mask reproduces wrist deformation");
        Check(render(fixed,hand,.75f,true,angle)==rigid,"mixed wrist vertices retain rigid shape through rotation");
        Check(render(fixed,hand,1,true,angle)==rigid,"fully weighted hand/gun/scope vertices unchanged");
        Check(render(fixed,hand,.75f,false,angle)==warped,"third person/inactive draw retains native pixels");
        for(float hidden:{0.f,.25f,.49f}) {
            const auto pixels=render(fixed,hand,hidden,true,angle);bool blank=true;
            for(size_t i=1;i<pixels.size();i+=4) if(pixels[i]) blank=false;
            Check(blank,"forearm and opposite-hand side remain hidden without stretched geometry");
        }
    }
    gl::UseProgram(0);gl::BindVertexArray(0);gl::DeleteBuffers(1,&vbo);gl::DeleteVertexArrays(1,&vao);gl::BindFramebuffer(GL_FRAMEBUFFER,0);gl::DeleteFramebuffers(1,&fbo);glDeleteTextures(1,&tex);
    gl::DeleteProgram(native);gl::DeleteProgram(fixed);for(auto s:{nativeVs,nativeFs,fixedVs,fixedFs}) gl::DeleteShader(s);
    Check(glGetError()==GL_NO_ERROR,"no GL errors after rendering and cleanup");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(core);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("OK: %d motion-hand GL checks passed.\n",checks);
}
