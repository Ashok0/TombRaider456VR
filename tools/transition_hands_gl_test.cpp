// Focused renderer regression: actual production cap extraction/drawing with a
// hidden GL window and synthetic mesh. No game or full self-test is executed.
#include "../src/BoneSkin.cpp"
#include "../src/FirstPersonVisibility.h"
#include <cstdarg>
#include <cstdlib>
static int checks=0;
static void Check(bool ok,const char* label) { ++checks;if(!ok) {std::printf("FAIL: %s\n",label);std::exit(1);} }
void Log(const char* s) { std::puts(s); }
void LogF(const char* f,...) {va_list args;va_start(args,f);std::vprintf(f,args);va_end(args);std::puts("");}
namespace tr {
static uint8_t appData[8192]{};
static const uint8_t* appPointer=appData;
static Layout layout{};
static float handPalette[33*12]{};
static bool sealing=true;
const Config& Cfg() { static Config c;return c; }
RenderState& VidState() { static RenderState state;return state; }
Shader* Shaders() { static Shader shaders[202];return shaders; }
bool DynamicBonesRenderBody() { return false; }
bool DynamicBonesWorldOffset(float*) { return false; }
bool DynamicBonesTorsoFrame(float*) { return false; }
bool DynamicBonesLaraForward(float*) { return false; }
const float* FirstPersonRenderBodyPalette(uint64_t&,int&) { return nullptr; }
uint64_t FirstPersonBodyWristSeamMask() { return 0; }
int FirstPersonTrackedHandJoint() { return sealing ? 10 : -1; }
uint64_t Base() { return reinterpret_cast<uint64_t>(&appPointer); }
const Layout& L() { return layout; }
uint64_t GameDllBase() { return 1; }
bool IsWorldPass() { return true; }
const float* FirstPersonSealedHandSkin(int& joint,float centre[3]) {
    joint=10;centre[0]=centre[1]=0;centre[2]=-.4f;return sealing ? handPalette : nullptr;
}
}
namespace hook {
InlineHook::~InlineHook() {}
void InlineHook::Remove() {}
bool InlineHook::Install(void*,void*,size_t,const uint8_t*,size_t,const char*,const int*,size_t) { return false; }
}
int main() {
    using namespace tr;
    for(int state:{19,54,55}) Check(firstperson::TraversalTransitionHandsOnly(state,firstperson::ArmMeshBits),"mounts show only hands");
    for(int state:{10,30,31,75,76,77,78,79,82,83,139})
        Check(!firstperson::TraversalTransitionHandsOnly(state,firstperson::ArmMeshBits),"hanging and monkey bars retain arms");
    firstperson::BodyVisibilityRecovery visibility;
    const auto mask=[&](double time,bool grip,bool recovering) {return visibility.Update(&visibility,0,time,grip,false,recovering);};
    mask(1,true,false);
    Check(firstperson::TraversalTransitionHandsOnly(2,mask(1.1,false,true)),"post-mount recovery hides arms");
    Check(firstperson::TraversalTransitionHandsOnly(71,firstperson::ArmMeshBits),"finishing a mount into crouch also seals the hands");
    Check(!firstperson::TraversalTransitionHandsOnly(71,UINT32_MAX),"ordinary crouching does not inherit transition cap scope");
    Check(firstperson::TraversalTransitionHandsOnly(2,mask(1.8,false,true)),"hands remain isolated throughout long recovery");
    Check(!firstperson::TraversalTransitionHandsOnly(2,mask(1.81,false,false)),"arms return when recovery finishes");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TransitionCapTest";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden GL window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Transition cap test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    HDC dc=GetDC(window);PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;
    Check(SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)!=0,"set GL format");
    HGLRC context=wglCreateContext(dc);
    Check(context && wglMakeCurrent(dc,context) && gl::Load() && gl::LoadedSkinApi(),"load real GL renderer");
    const char* vs=R"(#version 150
in vec3 aCoord;in vec4 aLight;in vec4 aColor;
uniform mat4 uProjMatrix;uniform vec4 uViewMatrix[3];out float weight;
void main() {weight=aLight.x==10.0 ? aColor.x : 0.0;
gl_Position=uProjMatrix*vec4(dot(uViewMatrix[0].xyz,aCoord),dot(uViewMatrix[1].xyz,aCoord),dot(uViewMatrix[2].xyz,aCoord),1.0);})";
    const char* fs=R"(#version 150
in float weight;out vec4 color;void main(){if(weight<0.5)discard;color=vec4(1.0);})";
    GLuint vertex=gl::CreateShader(GL_VERTEX_SHADER),fragment=gl::CreateShader(GL_FRAGMENT_SHADER);
    gl::ShaderSource(vertex,1,&vs,nullptr);gl::CompileShader(vertex);gl::ShaderSource(fragment,1,&fs,nullptr);gl::CompileShader(fragment);
    GLuint program=gl::CreateProgram();gl::AttachShader(program,vertex);gl::AttachShader(program,fragment);gl::LinkProgram(program);
    GLint linked=0;gl::GetProgramiv(program,GL_LINK_STATUS,&linked);Check(linked!=0,"mesh shader links");
    gl::UseProgram(program);Check(program<kMaxProgram,"program fits native cache");g_prog[program].handJoint=10;
    struct Vertex {float position[3];uint8_t joint[4],weight[4];} vertices[8]{};
    for(int i=0;i<8;++i) {
        vertices[i].position[0]=(i%4==0 || i%4==3) ? -.8f : .8f;
        vertices[i].position[1]=i%4<2 ? -.8f : .8f;
        vertices[i].position[2]=i<4 ? -.4f : .4f;
        vertices[i].joint[0]=10;vertices[i].joint[1]=9;
    }
    uint16_t indices[24]{};
    for(int i=0;i<4;++i) {const int j=(i+1)%4;const uint16_t face[6]={uint16_t(i),uint16_t(j),uint16_t(i+4),uint16_t(j),uint16_t(j+4),uint16_t(i+4)};std::memcpy(indices+i*6,face,sizeof(face));}
    GLuint vao=0,vbo=0,ibo=0;gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);gl::GenBuffers(1,&vbo);gl::GenBuffers(1,&ibo);
    gl::BindBuffer(GL_ARRAY_BUFFER,vbo);gl::BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);gl::BufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    for(int i=0;i<3;++i) {
        const char* names[]={"aCoord","aLight","aColor"};const size_t offsets[]={0,offsetof(Vertex,joint),offsetof(Vertex,weight)};
        const GLint loc=gl::GetAttribLocation(program,names[i]);Check(loc>=0,"mesh attribute active");
        gl::VertexAttribPointer(loc,i ? 4 : 3,i ? GL_UNSIGNED_BYTE : GL_FLOAT,i==2, sizeof(Vertex),reinterpret_cast<void*>(offsets[i]));gl::EnableVertexAttribArray(loc);
    }
    uint32_t mesh[5]{};mesh[4]=2;
    for(int j=0;j<33;++j) handPalette[j*12]=handPalette[j*12+5]=handPalette[j*12+10]=1;
    const float projection[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    gl::UniformMatrix4fv(gl::GetUniformLocation(program,"uProjMatrix"),1,GL_FALSE,projection);
    glViewport(0,0,8,8);glDisable(GL_DEPTH_TEST);
    for(int pass=0;pass<4;++pass) {
        if(pass%2==0) {
            ResetWristCaps();
            for(int i=0;i<8;++i) {vertices[i].weight[0]=(pass<2 && i<4) ? 0 : 255;vertices[i].weight[1]=255-vertices[i].weight[0];}
            gl::BindBuffer(GL_ARRAY_BUFFER,vbo);gl::BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        }
        const float view[12]={1,0,0,0,0,1,0,0,0,0,pass%2 ? -1.f : 1.f,0};
        gl::Uniform4fv(gl::GetUniformLocation(program,"uViewMatrix[0]"),3,view);
        glEnable(GL_BLEND);glEnable(GL_CULL_FACE);glDepthMask(GL_FALSE);glClearColor(1,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES,24,GL_UNSIGNED_SHORT,nullptr);
        unsigned char pixel[4]{};glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        Check(pixel[0]>240 && pixel[2]>240,"native open wrist exposes background");
        BoneSkinDrawWristCap(mesh,(pass%2)*12,12);
        glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        Check(pixel[0]<5 && pixel[1]<5 && pixel[2]<5,"production cap seals blended and open wrists opaque black from both sides");
        GLint p=0,v=0,b=0;GLboolean depth=GL_TRUE;
        glGetIntegerv(GL_CURRENT_PROGRAM,&p);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&v);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&b);glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);
        Check(p==GLint(program) && v==GLint(vao) && b==GLint(vbo) && !depth && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE),"cap restores native render state");
    }
    Check(g_wristRims.size()==1 && g_wristRims[0].loops.size()==2,"material ranges share true closed boundaries");
    sealing=false;glClear(GL_COLOR_BUFFER_BIT);BoneSkinDrawWristCap(mesh,0,24);
    unsigned char pixel[4]{};glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    Check(pixel[0]>240 && pixel[2]>240,"caps stop immediately when transition draw scope ends");
    Check(glGetError()==GL_NO_ERROR,"cap drawing leaves no GL errors");
    ResetWristCaps();Check(!g_capProgram && !g_capVao && !g_capBuffer && g_wristRims.empty(),"cap resources reset");
    gl::UseProgram(0);gl::BindVertexArray(0);gl::DeleteBuffers(1,&vbo);gl::DeleteBuffers(1,&ibo);gl::DeleteVertexArrays(1,&vao);gl::DeleteProgram(program);gl::DeleteShader(vertex);gl::DeleteShader(fragment);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("Transition hands: %d checks passed\n",checks);
}
