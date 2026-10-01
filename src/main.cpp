#include <windows.h>
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <wincodec.h>
#include <wrl/client.h>

// Only the OpenGL 3.3 entry points used by this project. No external loader.
using GLsizeiptr = ptrdiff_t;
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#define GL_CONTEXT_PROFILE_MASK 0x9126
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#define GLFN(ret, name, ...) using name##Type = ret (APIENTRY*)(__VA_ARGS__); name##Type name = nullptr
GLFN(void, glGenVertexArrays, GLsizei, GLuint*);
GLFN(void, glBindVertexArray, GLuint);
GLFN(void, glDeleteVertexArrays, GLsizei, const GLuint*);
GLFN(void, glGenBuffers, GLsizei, GLuint*);
GLFN(void, glBindBuffer, GLenum, GLuint);
GLFN(void, glBufferData, GLenum, GLsizeiptr, const void*, GLenum);
GLFN(void, glDeleteBuffers, GLsizei, const GLuint*);
GLFN(void, glEnableVertexAttribArray, GLuint);
GLFN(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
GLFN(GLuint, glCreateShader, GLenum);
GLFN(void, glShaderSource, GLuint, GLsizei, const char* const*, const GLint*);
GLFN(void, glCompileShader, GLuint);
GLFN(void, glGetShaderiv, GLuint, GLenum, GLint*);
GLFN(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei*, char*);
GLFN(GLuint, glCreateProgram, void);
GLFN(void, glAttachShader, GLuint, GLuint);
GLFN(void, glLinkProgram, GLuint);
GLFN(void, glGetProgramiv, GLuint, GLenum, GLint*);
GLFN(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei*, char*);
GLFN(void, glDeleteShader, GLuint);
GLFN(void, glDeleteProgram, GLuint);
GLFN(void, glUseProgram, GLuint);
GLFN(GLint, glGetUniformLocation, GLuint, const char*);
GLFN(void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat*);
GLFN(void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat);
GLFN(void, glUniform1f, GLint, GLfloat);
GLFN(void, glUniform1i, GLint, GLint);
GLFN(void, glActiveTexture, GLenum);
GLFN(void, glTexImage3D, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLFN(void, glTexSubImage3D, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
GLFN(void, glGenerateMipmap, GLenum);
GLFN(void, glGenFramebuffers, GLsizei, GLuint*);
GLFN(void, glBindFramebuffer, GLenum, GLuint);
GLFN(void, glDeleteFramebuffers, GLsizei, const GLuint*);
GLFN(void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint);
GLFN(GLenum, glCheckFramebufferStatus, GLenum);
GLFN(void, glDrawBuffers, GLsizei, const GLenum*);

template<class T> void loadGL(T& out, const char* name) {
    PROC p = wglGetProcAddress(name);
    if (!p || p == reinterpret_cast<PROC>(1) || p == reinterpret_cast<PROC>(2) ||
        p == reinterpret_cast<PROC>(3) || p == reinterpret_cast<PROC>(-1))
        throw std::runtime_error(std::string("OpenGL function unavailable: ") + name);
    out = reinterpret_cast<T>(p);
}
void loadOpenGL() {
#define LOAD(name) loadGL(name, #name)
    LOAD(glGenVertexArrays); LOAD(glBindVertexArray); LOAD(glDeleteVertexArrays);
    LOAD(glGenBuffers); LOAD(glBindBuffer); LOAD(glBufferData); LOAD(glDeleteBuffers);
    LOAD(glEnableVertexAttribArray); LOAD(glVertexAttribPointer); LOAD(glCreateShader);
    LOAD(glShaderSource); LOAD(glCompileShader); LOAD(glGetShaderiv); LOAD(glGetShaderInfoLog);
    LOAD(glCreateProgram); LOAD(glAttachShader); LOAD(glLinkProgram); LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog); LOAD(glDeleteShader); LOAD(glDeleteProgram); LOAD(glUseProgram);
    LOAD(glGetUniformLocation); LOAD(glUniformMatrix4fv); LOAD(glUniform3f); LOAD(glUniform1f); LOAD(glUniform1i);
    LOAD(glActiveTexture);LOAD(glTexImage3D);LOAD(glTexSubImage3D);LOAD(glGenerateMipmap);
    LOAD(glGenFramebuffers);LOAD(glBindFramebuffer);LOAD(glDeleteFramebuffers);
    LOAD(glFramebufferTexture2D);LOAD(glCheckFramebufferStatus);LOAD(glDrawBuffers);
#undef LOAD
}

#include "textures.h"

constexpr float PI = 3.14159265359f;
struct V3 { float x=0, y=0, z=0; };
V3 operator+(V3 a, V3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V3 operator*(V3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
float dot(V3 a,V3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V3 cross(V3 a,V3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float length(V3 a) { return std::sqrt(dot(a,a)); }
V3 normalized(V3 a) { float l=length(a); return l>0.00001f?a*(1/l):V3{}; }
float distance2D(V3 a,V3 b) { a.y=b.y=0; return length(a-b); }
struct Mat { float m[16]{}; };
Mat identity() { Mat a; a.m[0]=a.m[5]=a.m[10]=a.m[15]=1; return a; }
Mat operator*(const Mat& a,const Mat& b) {
    Mat c;
    for(int col=0;col<4;++col) for(int row=0;row<4;++row)
        for(int k=0;k<4;++k) c.m[col*4+row]+=a.m[k*4+row]*b.m[col*4+k];
    return c;
}
Mat perspective(float fov,float aspect) {
    Mat a; float f=1/std::tan(fov/2); a.m[0]=f/aspect; a.m[5]=f;
    a.m[10]=-120.08f/119.92f; a.m[11]=-1; a.m[14]=-19.2f/119.92f; return a;
}
Mat lookAt(V3 eye,V3 dir) {
    V3 f=normalized(dir),s=normalized(cross(f,{0,1,0})),u=cross(s,f);
    Mat a=identity();
    a.m[0]=s.x; a.m[4]=s.y; a.m[8]=s.z;
    a.m[1]=u.x; a.m[5]=u.y; a.m[9]=u.z;
    a.m[2]=-f.x; a.m[6]=-f.y; a.m[10]=-f.z;
    a.m[12]=-dot(s,eye); a.m[13]=-dot(u,eye); a.m[14]=dot(f,eye); return a;
}
struct Vertex {
    V3 p,n,c; V3 tex; float material=0;
    std::array<uint8_t,4> bones{};
    std::array<float,4> weights{};
};
using Mesh = std::vector<Vertex>;

struct SkinMatrix {
    float m[12]{};
    V3 apply(V3 p) const {
        return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],
                m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],
                m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};
    }
    V3 applyNormal(V3 n) const {
        return normalized({m[0]*n.x+m[1]*n.y+m[2]*n.z,
                           m[4]*n.x+m[5]*n.y+m[6]*n.z,
                           m[8]*n.x+m[9]*n.y+m[10]*n.z});
    }
};
struct RuntimeVertex {
    V3 p,n,c,tex;
    std::array<uint8_t,4> bones{};
    std::array<float,4> weights{};
    uint32_t material=0;
};
struct RuntimeClip {
    std::string name;
    uint32_t frames=0;
    float fps=30;
    std::vector<SkinMatrix> matrices;
};
struct RuntimeProfessor {
    uint32_t boneCount=0;
    std::vector<RuntimeVertex> vertices;
    Mesh mesh;
    std::vector<RuntimeClip> clips;
    bool loaded=false;
};
RuntimeProfessor runtimeProfessor;
#include "rig_animation.h"

template<class T> bool readBinary(std::ifstream& in,T& value) {
    return bool(in.read(reinterpret_cast<char*>(&value),std::streamsize(sizeof(T))));
}
bool loadRuntimeProfessor() {
    const std::array<std::filesystem::path,3> candidates={
        std::filesystem::path("assets/models/teacher_runtime.bin"),
        std::filesystem::path("../assets/models/teacher_runtime.bin"),
        std::filesystem::path("../../assets/models/teacher_runtime.bin")};
    std::ifstream in;
    for(const auto& candidate:candidates) if(std::filesystem::exists(candidate)) {
        in.clear();in.open(candidate,std::ios::binary); if(in) break;
    }
    if(!in) return false;
    char magic[4]{};uint32_t version=0,vertexCount=0,boneCount=0,clipCount=0;
    if(!in.read(magic,4)||!readBinary(in,version)||!readBinary(in,vertexCount)||
       !readBinary(in,boneCount)||!readBinary(in,clipCount)) return false;
    if(std::string(magic,4)!="TCH1"||version!=1||vertexCount==0||vertexCount>1000000||
       boneCount==0||boneCount>32||clipCount==0||clipCount>8) return false;
    RuntimeProfessor loaded;loaded.boneCount=boneCount;loaded.vertices.resize(vertexCount);
    for(auto& vertex:loaded.vertices) {
        if(!in.read(reinterpret_cast<char*>(&vertex.p),sizeof(V3))||
           !in.read(reinterpret_cast<char*>(&vertex.n),sizeof(V3))||
           !in.read(reinterpret_cast<char*>(&vertex.c),sizeof(V3))||
           !in.read(reinterpret_cast<char*>(&vertex.tex),sizeof(V3))||
           !in.read(reinterpret_cast<char*>(vertex.bones.data()),4)||
           !in.read(reinterpret_cast<char*>(vertex.weights.data()),sizeof(float)*4)||
           !readBinary(in,vertex.material)) return false;
    }
    loaded.clips.reserve(clipCount);
    for(uint32_t clipIndex=0;clipIndex<clipCount;++clipIndex) {
        char name[16]{};uint32_t frames=0;float fps=0;
        if(!in.read(name,sizeof(name))||!readBinary(in,frames)||!readBinary(in,fps)||
           frames==0||frames>600||fps<=0) return false;
        const size_t matrixCount=size_t(frames)*boneCount;
        if(matrixCount>50000) return false;
        size_t nameLength=0;while(nameLength<sizeof(name)&&name[nameLength])++nameLength;
        RuntimeClip clip;clip.name=std::string(name,nameLength);clip.frames=frames;clip.fps=fps;clip.matrices.resize(matrixCount);
        for(auto& matrix:clip.matrices) if(!in.read(reinterpret_cast<char*>(matrix.m),sizeof(matrix.m))) return false;
        loaded.clips.push_back(std::move(clip));
    }
    loaded.mesh.reserve(loaded.vertices.size());
    for(const auto& source:loaded.vertices)
        loaded.mesh.push_back({source.p,source.n,source.c,source.tex,float(source.material),source.bones,source.weights});
    loaded.loaded=true;runtimeProfessor=std::move(loaded);
    return true;
}
void quad(Mesh& m,V3 a,V3 b,V3 c,V3 d,V3 n,V3 color) {
    for(V3 p : {a,b,c,a,c,d}) m.push_back({p,n,color,p,0});
}

void box(Mesh& m,V3 p,V3 half,V3 color) {
    float x=p.x-half.x,X=p.x+half.x,y=p.y-half.y,Y=p.y+half.y,z=p.z-half.z,Z=p.z+half.z;
    quad(m,{x,y,Z},{X,y,Z},{X,Y,Z},{x,Y,Z},{0,0,1},color);
    quad(m,{X,y,z},{x,y,z},{x,Y,z},{X,Y,z},{0,0,-1},color);
    quad(m,{x,y,z},{x,y,Z},{x,Y,Z},{x,Y,z},{-1,0,0},color);
    quad(m,{X,y,Z},{X,y,z},{X,Y,z},{X,Y,Z},{1,0,0},color);
    quad(m,{x,Y,Z},{X,Y,Z},{X,Y,z},{x,Y,z},{0,1,0},color);
    quad(m,{x,y,z},{X,y,z},{X,y,Z},{x,y,Z},{0,-1,0},color);
}

#include "geometry.h"
#include "animation.h"

constexpr int N=19;
constexpr float CELL=3.2f;
using Grid=std::array<std::array<int,N>,N>;
Grid grid{};
std::array<std::array<bool,N>,N> explored{};
std::mt19937 rng;
struct Tile { int x=0,z=0; bool operator==(Tile b) const { return x==b.x && z==b.z; } };
const Tile dirs[]={{1,0},{-1,0},{0,1},{0,-1}};
bool inside(int x,int z) { return x>0 && z>0 && x<N-1 && z<N-1; }
bool walkable(int x,int z) { return inside(x,z) && grid[z][x]==0; }
V3 center(Tile t) { return {(t.x+0.5f)*CELL,0,(t.z+0.5f)*CELL}; }
Tile tile(V3 p) { return {int(std::floor(p.x/CELL)),int(std::floor(p.z/CELL))}; }
std::vector<Tile> route(Tile from,Tile to) {
    if(!walkable(from.x,from.z)||!walkable(to.x,to.z)) return {};
    std::array<std::array<int,N>,N> prev;
    for(auto& row:prev) row.fill(-1);
    std::queue<Tile> q; q.push(from); prev[from.z][from.x]=from.z*N+from.x;
    while(!q.empty()) {
        Tile a=q.front(); q.pop(); if(a==to) break;
        for(Tile d:dirs) { Tile b{a.x+d.x,a.z+d.z};
            if(walkable(b.x,b.z)&&prev[b.z][b.x]<0) { prev[b.z][b.x]=a.z*N+a.x; q.push(b); }
        }
    }
    if(prev[to.z][to.x]<0) return {};
    std::vector<Tile> path;
    for(Tile p=to;!(p==from);) { path.push_back(p); int k=prev[p.z][p.x]; p={k%N,k/N}; }
    std::reverse(path.begin(),path.end()); return path;
}
struct Note { Tile tile; bool taken=false; };
std::vector<Note> notes;
std::vector<Tile> openTiles;
Tile exitTile{1,1};
V3 player,teacher;
Tile noteWall(Tile t){for(Tile d:dirs)if(!walkable(t.x+d.x,t.z+d.z))return d;return {0,-1};}
V3 notePosition(const Note& n){Tile d=noteWall(n.tile);return center(n.tile)+V3{float(d.x)*1.43f,0,float(d.z)*1.43f};}
#include "lockers.h"
void makeMap(uint32_t seed) {
    rng.seed(seed); for(auto& r:grid) r.fill(1); for(auto& r:explored) r.fill(false);
    // A faculty floor has wings and cross-corridors, not random isolated wall cubes.
    for(int z:{3,9,15}) for(int x=1;x<N-1;++x) grid[z][x]=0;
    for(int x:{1,17})for(int z=1;z<N-1;++z)grid[z][x]=0;
    for(int wing=0;wing<2;++wing){
        int from=wing?9:3,to=wing?15:9;
        for(int base:{5,12}){int x=base+int(rng()%2);for(int z=from;z<=to;++z)grid[z][x]=0;}
    }
    for(int z=7;z<=11;++z) for(int x=7;x<=11;++x) grid[z][x]=0;
    for(int x:{3,7,11,15})for(int z:{1,2,16,17})grid[z][x]=0;
    openTiles.clear(); for(int z=1;z<N-1;++z) for(int x=1;x<N-1;++x) if(!grid[z][x]) openTiles.push_back({x,z});
    player=center({1,1});
    Tile farthest{1,1}; size_t farDist=0;
    for(Tile t:openTiles) { size_t dist=route({1,1},t).size(); if(dist>farDist) { farthest=t;farDist=dist; } }
    teacher=center(farthest); notes.clear();
    std::vector<Tile> selected{{1,1}};
    for(int i=0;i<5;++i) {
        Tile best=farthest; size_t bestScore=0;
        for(Tile t:openTiles) {
            bool nearWall=false;for(Tile d:dirs)nearWall|=!walkable(t.x+d.x,t.z+d.z);if(!nearWall)continue;
            size_t score=10000;
            for(Tile s:selected) score=std::min(score,route(s,t).size());
            if(score>bestScore) { bestScore=score; best=t; }
        }
        notes.push_back({best,false}); selected.push_back(best);
    }
    generateLockers();
}
bool canStand(V3 p) {
    constexpr float r=0.25f;
    for(const auto& l:lockers)if(insideLocker(p,l,r))return false;
    for(float x:{-r,r}) for(float z:{-r,r}) {
        Tile t=tile(p+V3{x,0,z}); if(!walkable(t.x,t.z)) return false;
    }
    return true;
}
void movePlayer(V3 delta) {
    // Small substeps keep collisions reliable even during a slow frame.
    int steps=std::max(1,int(length(delta)/0.10f)+1); delta=delta*(1.0f/steps);
    for(int i=0;i<steps;++i) {
        V3 p=player; p.x+=delta.x; if(canStand(p)) player=p;
        p=player; p.z+=delta.z; if(canStand(p)) player=p;
    }
}
bool lineOfSight(V3 a,V3 b) {
    int samples=std::max(1,int(distance2D(a,b)/.08f));
    for(int i=0;i<=samples;++i)for(const auto& l:lockers)
        if(insideLocker(a+(b-a)*(float(i)/samples),l))return false;
    // Exact segment/slab intersections also detect a very short cut across a corner.
    Tile lo=tile({std::min(a.x,b.x),0,std::min(a.z,b.z)});
    Tile hi=tile({std::max(a.x,b.x),0,std::max(a.z,b.z)});
    for(int z=lo.z;z<=hi.z;++z)for(int x=lo.x;x<=hi.x;++x){
        if(walkable(x,z))continue;
        float enter=0,leave=1;
        auto slab=[&](float origin,float delta,float low,float high){
            if(std::abs(delta)<1e-7f)return origin>=low&&origin<=high;
            float t0=(low-origin)/delta,t1=(high-origin)/delta;
            if(t0>t1)std::swap(t0,t1);
            enter=std::max(enter,t0);leave=std::min(leave,t1);return enter<=leave;
        };
        if(slab(a.x,b.x-a.x,x*CELL,(x+1)*CELL)&&slab(a.z,b.z-a.z,z*CELL,(z+1)*CELL))return false;
    }
    return true;
}

bool clearWalkSegment(V3 a,V3 b) {
    int steps=std::max(1,int(distance2D(a,b)/0.09f)+1);
    for(int i=0;i<=steps;++i)if(!canStand(a+(b-a)*(float(i)/steps)))return false;
    return true;
}
bool caughtDuringStep(V3 playerBefore,V3 playerAfter,V3 teacherBefore,V3 teacherAfter) {
    V3 offset=playerBefore-teacherBefore,relative=(playerAfter-playerBefore)-(teacherAfter-teacherBefore);
    offset.y=relative.y=0;
    float speedSquared=dot(relative,relative);
    float closest=speedSquared>0.000001f?std::clamp(-dot(offset,relative)/speedSquared,0.0f,1.0f):0;
    for(float t:{0.0f,closest,1.0f}){
        V3 p=playerBefore+(playerAfter-playerBefore)*t,q=teacherBefore+(teacherAfter-teacherBefore)*t;
        if(distance2D(p,q)<=0.82f && lineOfSight(p,q))return true;
    }
    return false;
}

HWND windowHandle=nullptr;
HDC deviceContext=nullptr;
HGLRC renderContext=nullptr;
int screenW=1280,screenH=800;
bool running=true,focused=true,captured=false,fullscreen=false;
bool keys[256]{},pressed[256]{};
float mouseDX=0,mouseDY=0;
WINDOWPLACEMENT previousPlacement{sizeof(WINDOWPLACEMENT)};
enum class Mode { Title,Playing,Paused,Won,Lost };
Mode mode=Mode::Title;
float yaw=0,pitch=0,stamina=1,gameTime=0,alert=0,notice=0,aiTick=0,footPhase=0;
bool lamp=true,mapVisible=true,exhausted=false,sprinting=false;
int collected=0;
uint32_t currentSeed=2026;
std::string noticeText;
Tile target{};
std::vector<Tile> teacherPath;
Mesh staticMesh;
GLuint vao=0,vbo=0,program=0;
GLuint runtimeVao=0,runtimeVbo=0;
GLuint worldVao=0,worldVbo=0;
GLint locMatrix,locView,locEye,locForward,locUI,locLamp,locTeacher,locSkinEnabled,locSkinOffset;
GLint locBones[32]{};
GLint locLights[12]{},locLightCount;
void captureMouse(bool value) {
    if(captured==value) return;
    captured=value;
    if(value) {
        RECT r; GetClientRect(windowHandle,&r); POINT a{r.left,r.top},b{r.right,r.bottom};
        ClientToScreen(windowHandle,&a);ClientToScreen(windowHandle,&b);
        RECT clip{a.x,a.y,b.x,b.y}; ClipCursor(&clip);
        SetCursorPos((a.x+b.x)/2,(a.y+b.y)/2); ShowCursor(FALSE);
    } else { ClipCursor(nullptr); ShowCursor(TRUE); }
    mouseDX=mouseDY=0;
}
void toggleFullscreen() {
    if(!fullscreen) {
        GetWindowPlacement(windowHandle,&previousPlacement);
        MONITORINFO info{sizeof(info)}; GetMonitorInfo(MonitorFromWindow(windowHandle,MONITOR_DEFAULTTONEAREST),&info);
        SetWindowLongPtr(windowHandle,GWL_STYLE,WS_POPUP|WS_VISIBLE);
        SetWindowPos(windowHandle,HWND_TOP,info.rcMonitor.left,info.rcMonitor.top,
                     info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top,SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtr(windowHandle,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
        SetWindowPlacement(windowHandle,&previousPlacement);
        SetWindowPos(windowHandle,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    }
    fullscreen=!fullscreen;
    if(captured) { captureMouse(false); captureMouse(true); }
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CLOSE: running=false; return 0;
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={800,550};return 0;
    }
    case WM_SIZE:
        screenW=std::max(1,int(LOWORD(lp)));screenH=std::max(1,int(HIWORD(lp)));
        if(captured) { captureMouse(false); captureMouse(true); } return 0;
    case WM_ACTIVATEAPP:
        focused=wp!=0;
        if(!focused) { if(mode==Mode::Playing) mode=Mode::Paused; captureMouse(false); std::fill(std::begin(keys),std::end(keys),false); }
        return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if(msg==WM_SYSKEYDOWN && wp==VK_F4) return DefWindowProc(hwnd,msg,wp,lp);
        if(wp<256) { if(!keys[wp]) pressed[wp]=true; keys[wp]=true; } return 0;
    case WM_KEYUP: case WM_SYSKEYUP: if(wp<256) keys[wp]=false; return 0;
    case WM_INPUT: {
        RAWINPUT data{}; UINT size=sizeof(data);
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,&data,&size,sizeof(RAWINPUTHEADER))==size && captured && data.header.dwType==RIM_TYPEMOUSE) {
            mouseDX+=float(data.data.mouse.lLastX);mouseDY+=float(data.data.mouse.lLastY);
        }
        return DefWindowProc(hwnd,msg,wp,lp);
    }
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}
void createWindowAndGL(bool hidden) {
    WNDCLASS wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=windowProc;wc.hInstance=GetModuleHandle(nullptr);
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"BiophysicsEscapeWindow";
    RegisterClass(&wc);
    RECT r{0,0,screenW,screenH};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    windowHandle=CreateWindow(wc.lpszClassName,L"Побег с биофизики | МГУ - OpenGL 3.3",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!windowHandle) throw std::runtime_error("Cannot create a Win32 window.");
    deviceContext=GetDC(windowHandle);
    PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;
    int format=ChoosePixelFormat(deviceContext,&pfd);
    if(!format||!SetPixelFormat(deviceContext,format,&pfd)) throw std::runtime_error("No compatible OpenGL pixel format.");
    HGLRC legacy=wglCreateContext(deviceContext);
    if(!legacy||!wglMakeCurrent(deviceContext,legacy)) throw std::runtime_error("Cannot initialize WGL.");
    using CreateContext = HGLRC (WINAPI*)(HDC,HGLRC,const int*);
    CreateContext create=nullptr;
    try { loadGL(create,"wglCreateContextAttribsARB"); }
    catch(...) { wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);throw; }
    const int attributes[]={0x2091,3,0x2092,3,0x9126,1,0};
    renderContext=create(deviceContext,nullptr,attributes);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);
    if(!renderContext||!wglMakeCurrent(deviceContext,renderContext)) throw std::runtime_error("OpenGL 3.3 Core is required. Update your GPU driver.");
    loadOpenGL();
    using SwapInterval=BOOL(WINAPI*)(int); auto swap=reinterpret_cast<SwapInterval>(wglGetProcAddress("wglSwapIntervalEXT"));
    if(swap) swap(1);
    RAWINPUTDEVICE input{0x01,0x02,0,windowHandle};RegisterRawInputDevices(&input,1,sizeof(input));
    if(!hidden) { ShowWindow(windowHandle,SW_SHOW);UpdateWindow(windowHandle); }
}

GLuint compileShader(GLenum kind,const char* source) {
    GLuint s=glCreateShader(kind);glShaderSource(s,1,&source,nullptr);glCompileShader(s);
    GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]{};glGetShaderInfoLog(s,sizeof(log),nullptr,log);glDeleteShader(s);throw std::runtime_error(log); }
    return s;
}
#include "ssao.h"

void vertexLayout() {
    for(GLuint i=0;i<3;++i) { glEnableVertexAttribArray(i);glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i)*sizeof(V3))); }
    glEnableVertexAttribArray(3);glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,tex)));
    glEnableVertexAttribArray(4);glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,material)));
    glEnableVertexAttribArray(5);glVertexAttribPointer(5,4,GL_UNSIGNED_BYTE,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,bones)));
    glEnableVertexAttribArray(6);glVertexAttribPointer(6,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,weights)));
}
void initRenderer() {
    const char* vs=R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(location=3) in vec3 aTex;
layout(location=4) in float aMaterial;
layout(location=5) in vec4 aBones;
layout(location=6) in vec4 aWeights;
uniform mat4 uMatrix;
uniform int uSkinEnabled;
uniform vec3 uSkinOffset;
uniform mat4 uBones[32];
out vec3 world; out vec3 normal; out vec3 color; out vec3 tex;
flat out int material;
void main(){
    vec3 position=aPos; vec3 surfaceNormal=aNormal;
    if(uSkinEnabled!=0){
        vec4 skinned=vec4(0.0);vec3 skinnedNormal=vec3(0.0);
        for(int i=0;i<4;++i){
            int bone=int(aBones[i]+0.5);float weight=aWeights[i];
            if(weight>0.0){ skinned+=(uBones[bone]*vec4(aPos,1.0))*weight; skinnedNormal+=(mat3(uBones[bone])*aNormal)*weight; }
        }
        position=skinned.xyz+uSkinOffset;surfaceNormal=normalize(skinnedNormal);
    }
    world=position; normal=surfaceNormal; color=aColor; tex=aTex; material=int(aMaterial+0.5);
    gl_Position=uMatrix*vec4(position,1.0);
}
)";
    const char* fs=materialShader;
    GLuint v=compileShader(GL_VERTEX_SHADER,vs),f=compileShader(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    glDeleteShader(v);glDeleteShader(f);
    GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok) { char log[4096]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);throw std::runtime_error(log); }
    locMatrix=glGetUniformLocation(program,"uMatrix");locEye=glGetUniformLocation(program,"uEye");
    locView=glGetUniformLocation(program,"uView");
    locForward=glGetUniformLocation(program,"uForward");locUI=glGetUniformLocation(program,"uUI");locLamp=glGetUniformLocation(program,"uLamp");
    locTeacher=glGetUniformLocation(program,"uTeacher");
    locSkinEnabled=glGetUniformLocation(program,"uSkinEnabled");locSkinOffset=glGetUniformLocation(program,"uSkinOffset");
    for(uint32_t i=0;i<32;++i)locBones[i]=glGetUniformLocation(program,("uBones["+std::to_string(i)+"]").c_str());
    locLightCount=glGetUniformLocation(program,"uLightCount");for(int i=0;i<12;++i)locLights[i]=glGetUniformLocation(program,("uLights["+std::to_string(i)+"]").c_str());
    glUseProgram(program);loadMaterials(program);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    vertexLayout();
    glGenVertexArrays(1,&worldVao);glBindVertexArray(worldVao);glGenBuffers(1,&worldVbo);glBindBuffer(GL_ARRAY_BUFFER,worldVbo);vertexLayout();
    if(runtimeProfessor.loaded){
        glGenVertexArrays(1,&runtimeVao);glBindVertexArray(runtimeVao);glGenBuffers(1,&runtimeVbo);glBindBuffer(GL_ARRAY_BUFFER,runtimeVbo);
        vertexLayout();glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(runtimeProfessor.mesh.size()*sizeof(Vertex)),runtimeProfessor.mesh.data(),GL_STATIC_DRAW);
    }
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glUniform1i(locSkinEnabled,0);glUniform3f(locSkinOffset,0,0,0);
    initSSAO();
}
void drawMesh(const Mesh& m) {
    if(m.empty()) return;
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(m.size()*sizeof(Vertex)),m.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES,0,GLsizei(m.size()));
}
std::array<uint8_t,7> glyph(char c);
void wallText(Mesh& mesh,V3 upperLeft,const std::string& value,float size,V3 color) {
    float offset=0;
    for(char c:value) {
        auto rows=glyph(c);
        for(int row=0;row<7;++row) for(int col=0;col<5;++col) if(rows[row]&(1<<(4-col)))
            {
                V3 p=upperLeft+V3{offset+col*size,-row*size,0};float h=size*0.47f;
                quad(mesh,p+V3{-h,-h,0},p+V3{h,-h,0},p+V3{h,h,0},p+V3{-h,h,0},{0,0,1},color);
            }
        offset+=6*size;
    }
}
std::array<uint8_t,7> russianGlyph(wchar_t c) {
    const std::wstring ru=L"АВЕКМНОРСТХ",latin=L"ABEKMHOPCTX";
    auto index=ru.find(c);if(index!=std::wstring::npos)return glyph(char(latin[index]));
    switch(c) {
    case L'Б':return {31,16,16,30,17,17,30};case L'И':return {17,17,19,21,25,17,17};
    case L'Й':return {10,4,17,19,21,25,17};case L'З':return {14,17,1,6,1,17,14};
    case L'Ф':return {4,14,21,21,21,14,4};case L'Л':return {7,9,9,9,9,17,17};
    case L'У':return {17,17,17,15,1,17,14};case L'Ь':return {16,16,16,30,17,17,30};
    case L'Ч':return {17,17,17,15,1,1,1};case L'Д':return {6,10,10,10,10,31,17};
    case L'П':return {31,17,17,17,17,17,17};case L'Г':return {31,16,16,16,16,16,16};
    case L'Э':return {14,17,1,7,1,17,14};case L'Ж':return {21,21,14,4,14,21,21};
    default:return c<128?glyph(char(c)):std::array<uint8_t,7>{};
    }
}
void russianSign(Mesh& mesh,V3 upperLeft,const std::wstring& value,float size,V3 color) {
    float offset=0;
    for(wchar_t c:value) {
        auto rows=russianGlyph(c);
        for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1<<(4-col))) {
            V3 p=upperLeft+V3{offset+col*size,-row*size,0};float h=size*0.46f;
            quad(mesh,p+V3{-h,-h,0},p+V3{h,-h,0},p+V3{h,h,0},p+V3{-h,h,0},{0,0,1},color);
        }
        offset+=size*6;
    }
}
#include "world.h"

void restart(bool newSeed) {
    if(newSeed) currentSeed=uint32_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    makeMap(currentSeed);buildWorld();
    professorAnimation={};
    yaw=walkable(2,1)?PI/2:PI;pitch=0;stamina=1;gameTime=0;alert=0;notice=5;noticeText="FIND 5 LAB REPORTS. RETURN TO THE GREEN EXIT.";
    aiTick=0;footPhase=0;collected=0;exhausted=false;teacherPath.clear();target=tile(teacher);
    mode=Mode::Playing;captureMouse(true);
}
V3 forward() { return {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)}; }
bool professorSees(V3 p){
    V3 delta=p-teacher;delta.y=0;float d=length(delta);
    V3 gaze{-std::sin(professorAnimation.facing),0,-std::cos(professorAnimation.facing)};
    return d<19&&lineOfSight(teacher,p)&&(d<.001f||dot(normalized(delta),gaze)>.5f);
}
int nearbyLocker(){
    if(hiddenLocker>=0)return hiddenLocker;
    for(size_t i=0;i<lockers.size();++i){const auto& l=lockers[i];
        if(distance2D(player,lockerApproach(l))<1.1f&&dot(forward(),lockerOut(l))>.35f)return int(i);
    }
    return -1;
}
void useLocker(){
    if(hiddenLocker>=0){
        player=lockerApproach(lockers[hiddenLocker]);lockers[hiddenLocker].door=1;hiddenLocker=-1;
        return;
    }
    int index=nearbyLocker();if(index<0)return;
    // Pursuit memory is not evidence of seeing the player enter this locker.
    bool spotted=professorSees(player);
    hiddenLocker=index;player=lockerPosition(lockers[index]);
    V3 out=lockerOut(lockers[index]);yaw=std::atan2(-out.x,out.z);pitch=0;
    sprinting=false;lockers[index].door=.65f;
    if(spotted){knownLocker=index;target=lockers[index].tile;alert=7;aiTick=0;teacherPath.clear();}
    notice=3;noticeText=spotted?"HE SAW YOU HIDE. GET OUT!":"HIDDEN. E TO LEAVE THE LOCKER.";
}
void update(float dt) {
    if(pressed[VK_F11]) toggleFullscreen();
    if(pressed[VK_ESCAPE]) {
        if(mode==Mode::Playing) { mode=Mode::Paused;captureMouse(false); }
        else if(mode==Mode::Paused) { mode=Mode::Playing;captureMouse(true); }
        else running=false;
    }
    if(mode!=Mode::Playing) {
        mouseDX=mouseDY=0;
        if(pressed[VK_RETURN]) { if(mode==Mode::Paused) { mode=Mode::Playing;captureMouse(true); } else restart(false); }
        if(pressed['R']) restart(true);
        return;
    }
    if(pressed['R']) { restart(true);return; }
    yaw+=mouseDX*0.0021f;pitch=std::clamp(pitch-mouseDY*0.0021f,-1.32f,1.32f);mouseDX=mouseDY=0;
    yaw+=(keys[VK_RIGHT]-keys[VK_LEFT])*dt*1.6f;
    pitch=std::clamp(pitch+(keys[VK_UP]-keys[VK_DOWN])*dt*1.2f,-1.32f,1.32f);
    if(pressed['F']) lamp=!lamp;
    if(pressed['M']) mapVisible=!mapVisible;
    if(pressed['E'])useLocker();
    if(hiddenLocker>=0){V3 out=lockerOut(lockers[hiddenLocker]);float base=std::atan2(-out.x,out.z);
        yaw=base+std::clamp(angleDelta(yaw,base),-.65f,.65f);pitch=std::clamp(pitch,-.45f,.45f);}
    float front=float(keys['W']-keys['S']),side=float(keys['D']-keys['A']);
    V3 motion{std::sin(yaw)*front+std::cos(yaw)*side,0,-std::cos(yaw)*front+std::sin(yaw)*side};
    bool moving=hiddenLocker<0&&length(motion)>0.1f;
    if(stamina<=0.005f) exhausted=true; if(stamina>0.30f) exhausted=false;
    sprinting=keys[VK_SHIFT]&&moving&&!exhausted&&stamina>0;
    stamina=std::clamp(stamina+(sprinting?-0.18f:0.115f)*dt,0.0f,1.0f);
    V3 previousPlayer=player;
    if(moving) { movePlayer(normalized(motion)*((sprinting?5.0f:2.8f)*dt));footPhase+=dt*(sprinting?14:9); }
    gameTime+=dt;notice=std::max(0.0f,notice-dt);
    Tile pt=tile(player);
    for(int z=std::max(0,pt.z-3);z<=std::min(N-1,pt.z+3);++z)
        for(int x=std::max(0,pt.x-3);x<=std::min(N-1,pt.x+3);++x) explored[z][x]=true;
    for(auto& n:notes) if(hiddenLocker<0&&!n.taken&&distance2D(player,notePosition(n))<1.5f) {
        n.taken=true;++collected;notice=4;
        const char* topics[]={"MECHANICS","ELECTRODYNAMICS","OPTICS","THERMODYNAMICS","QUANTUM PHYSICS"};
        noticeText=collected==5?"ALL REPORTS FOUND! RETURN TO THE GREEN EXIT.":std::string("REPORT RECOVERED: ")+topics[collected-1];
    }
    bool seen=hiddenLocker<0&&professorSees(player);
    if(seen) { alert=7;target=pt;knownLocker=-1;lockerSearch=0; }
    else if(knownLocker>=0){alert=7;target=lockers[knownLocker].tile;}
    else alert=std::max(0.0f,alert-dt);
    aiTick-=dt;
    if(aiTick<=0) {
        aiTick=0.40f;
        if(alert<=0 && (teacherPath.empty()||tile(teacher)==target)) target=openTiles[rng()%openTiles.size()];
        // Finish the current corridor segment before rerouting: no diagonal wall cuts.
        Tile anchor=teacherPath.empty()?tile(teacher):teacherPath.front();
        auto tail=route(anchor,target);
        teacherPath.clear();
        if(distance2D(teacher,center(anchor))>0.05f) teacherPath.push_back(anchor);
        teacherPath.insert(teacherPath.end(),tail.begin(),tail.end());
    }
    V3 previousTeacher=teacher;
    if(gameTime>6) {
        float budget=(alert>0?3.15f+collected*0.06f:1.45f)*dt;
        V3 destination=knownLocker>=0?lockerApproach(lockers[knownLocker]):player;
        if((seen||knownLocker>=0) && clearWalkSegment(teacher,destination)) {
            // Follow the exact position, including the edges of the same grid cell.
            V3 delta=destination-teacher;teacher=teacher+normalized(delta)*std::min(budget,length(delta));
            teacherPath.clear();
        } else while(budget>0&&!teacherPath.empty()) {
            V3 goal=center(teacherPath.front()),delta=goal-teacher;float distance=length(delta);
            float step=std::min(budget,distance);V3 next=teacher+normalized(delta)*step;
            if(!clearWalkSegment(teacher,next)){teacherPath.clear();aiTick=0;break;}
            teacher=next;budget-=step;
            if(distance<=step+0.0001f){teacher=goal;teacherPath.erase(teacherPath.begin());}else break;
        }
    }
    advanceProfessor(professorAnimation,previousTeacher,teacher,player,dt,alert>0,seen);
    bool opening=gameTime>6&&knownLocker>=0&&distance2D(teacher,lockerApproach(lockers[knownLocker]))<.15f;
    if(opening){
        lockerSearch+=dt;V3 out=lockerOut(lockers[knownLocker]);
        float desired=std::atan2(-out.x,-out.z);
        professorAnimation.facing+=std::clamp(angleDelta(desired,professorAnimation.facing),-dt*4.5f,dt*4.5f);
        professorAnimation.reach=std::min(1.f,lockerSearch*2);
    }
    for(size_t i=0;i<lockers.size();++i){float goal=opening&&int(i)==knownLocker?smoothStep(.3f,1.4f,lockerSearch):0;
        auto& door=lockers[i].door;door+=(goal-door)*(1-std::exp(-dt*7));}
    if(opening&&lockerSearch>1.8f&&lockers[knownLocker].door>.90f){
        if(hiddenLocker==knownLocker){mode=Mode::Lost;captureMouse(false);}
        knownLocker=-1;lockerSearch=0;
    }
    if(hiddenLocker<0&&gameTime>6&&caughtDuringStep(previousPlayer,player,previousTeacher,teacher)) { mode=Mode::Lost;captureMouse(false); }
    else if(hiddenLocker<0&&collected==5&&distance2D(player,center(exitTile))<1.1f) { mode=Mode::Won;captureMouse(false); }
}

void drawSkinnedRuntimeProfessor(V3 position,const ProfessorAnimation& animation) {
    if(!runtimeProfessor.loaded||runtimeProfessor.clips.empty()||!runtimeVao) return;
    auto findClip=[&](const char* name)->const RuntimeClip* {
        for(const auto& candidate:runtimeProfessor.clips) if(candidate.name==name) return &candidate;
        return nullptr;
    };
    const RuntimeClip* idleClip=findClip("Idle");
    const RuntimeClip* walkClip=findClip("Walk");
    const RuntimeClip* runClip=findClip("Run");
    const RuntimeClip* catchClip=findClip("Catch");
    if(!idleClip) idleClip=&runtimeProfessor.clips.front();
    if(!walkClip) walkClip=idleClip;
    if(!runClip) runClip=walkClip;
    if(!catchClip) catchClip=idleClip;
    const float idleFrame=animation.time*idleClip->fps;
    const float walkFrame=animation.phase/(2*PI)*float(walkClip->frames-1);
    const float runFrame=animation.phase/(2*PI)*float(runClip->frames-1);
    const float locomotion=smoothStep(0,1,std::clamp(animation.blend,0.0f,1.0f));
    const float catchFrame=animation.reach*float(catchClip->frames-1);
    std::vector<SkinMatrix> pose(runtimeProfessor.boneCount);
    for(uint32_t bone=0;bone<runtimeProfessor.boneCount;++bone) {
        auto idle=sampleSkin(*idleClip,idleFrame,bone,runtimeProfessor.boneCount);
        auto walk=sampleSkin(*walkClip,walkFrame,bone,runtimeProfessor.boneCount);
        auto run=sampleSkin(*runClip,runFrame,bone,runtimeProfessor.boneCount);
        pose[bone]=blendSkin(idle,blendSkin(walk,run,animation.runBlend),locomotion);
    }
    if(animation.reach>.001f&&runtimeProfessor.boneCount>=14){
        // Keep the pelvis and feet in locomotion while the upper body reaches.
        V3 chestBind{0,1.27f,0};
        auto catchChest=sampleSkin(*catchClip,catchFrame,3,runtimeProfessor.boneCount,false);
        V3 align=pose[3].apply(chestBind)-catchChest.apply(chestBind);
        for(uint32_t bone=3;bone<14;++bone){
            auto reaching=sampleSkin(*catchClip,catchFrame,bone,runtimeProfessor.boneCount,false);
            reaching.m[3]+=align.x;reaching.m[7]+=align.y;reaching.m[11]+=align.z;
            pose[bone]=blendSkin(pose[bone],reaching,smoothStep(0,.28f,animation.reach));
        }
    }
    const float facing=animation.facing,c=std::cos(facing),s=std::sin(facing);
    glUniform1i(locSkinEnabled,1);glUniform3f(locSkinOffset,position.x,position.y,position.z);
    for(uint32_t bone=0;bone<runtimeProfessor.boneCount;++bone) {
        SkinMatrix oriented=pose[bone];
        for(int col=0;col<4;++col) {
            oriented.m[col]=c*pose[bone].m[col]+s*pose[bone].m[8+col];
            oriented.m[8+col]=-s*pose[bone].m[col]+c*pose[bone].m[8+col];
        }
        float matrix[16]{};
        matrix[0]=oriented.m[0];matrix[4]=oriented.m[1];matrix[8]=oriented.m[2];matrix[12]=oriented.m[3];
        matrix[1]=oriented.m[4];matrix[5]=oriented.m[5];matrix[9]=oriented.m[6];matrix[13]=oriented.m[7];
        matrix[2]=oriented.m[8];matrix[6]=oriented.m[9];matrix[10]=oriented.m[10];matrix[14]=oriented.m[11];matrix[15]=1;
        glUniformMatrix4fv(locBones[bone],1,GL_FALSE,matrix);
    }
    glBindVertexArray(runtimeVao);glDrawArrays(GL_TRIANGLES,0,GLsizei(runtimeProfessor.mesh.size()));
    glUniform1i(locSkinEnabled,0);glUniform3f(locSkinOffset,0,0,0);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
}

// Compact, deliberately pixel-style font, rendered as triangles in the core profile.
std::array<uint8_t,7> glyph(char c) {
    switch(c) {
#define G(ch,a,b,c,d,e,f,g) case ch:return {a,b,c,d,e,f,g}
    G('A',14,17,17,31,17,17,17);G('B',30,17,17,30,17,17,30);G('C',14,17,16,16,16,17,14);
    G('D',30,17,17,17,17,17,30);G('E',31,16,16,30,16,16,31);G('F',31,16,16,30,16,16,16);
    G('G',14,17,16,23,17,17,15);G('H',17,17,17,31,17,17,17);G('I',31,4,4,4,4,4,31);
    G('J',7,2,2,2,18,18,12);G('K',17,18,20,24,20,18,17);G('L',16,16,16,16,16,16,31);
    G('M',17,27,21,21,17,17,17);G('N',17,25,21,19,17,17,17);G('O',14,17,17,17,17,17,14);
    G('P',30,17,17,30,16,16,16);G('Q',14,17,17,17,21,18,13);G('R',30,17,17,30,20,18,17);
    G('S',15,16,16,14,1,1,30);G('T',31,4,4,4,4,4,4);G('U',17,17,17,17,17,17,14);
    G('V',17,17,17,17,17,10,4);G('W',17,17,17,21,21,21,10);G('X',17,17,10,4,10,17,17);
    G('Y',17,17,10,4,4,4,4);G('Z',31,1,2,4,8,16,31);
    G('0',14,17,19,21,25,17,14);G('1',4,12,4,4,4,4,14);G('2',14,17,1,2,4,8,31);
    G('3',30,1,1,14,1,1,30);G('4',2,6,10,18,31,2,2);G('5',31,16,16,30,1,1,30);
    G('6',14,16,16,30,17,17,14);G('7',31,1,2,4,8,8,8);G('8',14,17,17,14,17,17,14);
    G('9',14,17,17,15,1,1,14);G(':',0,4,4,0,4,4,0);G('.',0,0,0,0,0,4,4);
    G('/',1,2,2,4,8,8,16);G('-',0,0,0,31,0,0,0);G('+',0,4,4,31,4,4,0);
    G('!',4,4,4,4,4,0,4);G('?',14,17,1,2,4,0,4);G('>',16,8,4,2,4,8,16);
    G('[',14,8,8,8,8,8,14);G(']',14,2,2,2,2,2,14);G('=',0,31,0,31,0,0,0);
#undef G
    default:return {};
    }
}
constexpr float UIW=1280,UIH=800;
void rect(Mesh& m,float x,float y,float w,float h,V3 c) {
    auto point=[](float X,float Y){return V3{X/UIW*2-1,1-Y/UIH*2,0};};
    quad(m,point(x,y),point(x+w,y),point(x+w,y+h),point(x,y+h),{0,0,1},c);
}
void text(Mesh& m,float x,float y,const std::string& s,float scale,V3 c) {
    float start=x;
    for(char ch:s) {
        if(ch=='\n') { y+=10*scale;x=start;continue; }
        auto g=glyph(ch);
        for(int row=0;row<7;++row) for(int col=0;col<5;++col) if(g[row]&(1<<(4-col))) rect(m,x+col*scale,y+row*scale,scale,scale,c);
        x+=6*scale;
    }
}
const V3 ink{0.035f,0.065f,0.080f},paper{0.88f,0.89f,0.80f},muted{0.43f,0.60f,0.62f},mint{0.32f,0.91f,0.73f},amber{1.0f,0.69f,0.30f},red{1.0f,0.32f,0.26f};
std::string elapsed() { int s=int(gameTime);char b[32];std::snprintf(b,sizeof(b),"%02d:%02d",s/60,s%60);return b; }
void drawUI() {
    static Mesh ui;ui.clear();ui.reserve(45000);
    bool playing=mode==Mode::Playing;
    rect(ui,28,26,355,97,ink);rect(ui,28,26,4,97,mint);
    text(ui,49,44,"MSU / PHYSICS",2,muted);
    text(ui,48,77,"REPORTS  "+std::to_string(collected)+" / 5",3,paper);
    rect(ui,28,697,288,75,ink);text(ui,47,712,exhausted?"CATCH YOUR BREATH":"STAMINA / SHIFT",1.8f,exhausted?amber:muted);
    rect(ui,47,743,249,7,{0.12f,0.20f,0.23f});rect(ui,47,743,249*stamina,7,exhausted?amber:mint);
    text(ui,345,729,"E HIDE / LEAVE   F LIGHT   M MAP   ESC PAUSE",1.7f,muted);
    rect(ui,1074,26,178,65,ink);text(ui,1093,46,elapsed(),3,paper);
    if(playing) {
        if(nearbyLocker()>=0){rect(ui,395,560,490,40,ink);text(ui,415,574,hiddenLocker>=0?"E LEAVE LOCKER":"E HIDE IN LOCKER",2,paper);}
        rect(ui,636,399,8,2,paper);rect(ui,639,396,2,8,paper);
        if(mapVisible) {
            float mx=1078,my=558,sz=8.2f;
            rect(ui,mx-18,my-32,192,246,ink);text(ui,mx,my-18,"FLOOR 03",1.7f,muted);
            for(int z=0;z<N;++z) for(int x=0;x<N;++x) if(explored[z][x]) rect(ui,mx+x*sz,my+z*sz,sz-0.7f,sz-0.7f,grid[z][x]?V3{0.19f,0.30f,0.32f}:V3{0.08f,0.15f,0.18f});
            for(auto n:notes) if(!n.taken && explored[n.tile.z][n.tile.x]) rect(ui,mx+(n.tile.x+0.25f)*sz,my+(n.tile.z+0.25f)*sz,sz/2,sz/2,amber);
            rect(ui,mx+exitTile.x*sz,my+exitTile.z*sz,sz-1,sz-1,mint);
            float px=mx+player.x/CELL*sz,pz=my+player.z/CELL*sz;
            rect(ui,px-2.4f,pz-2.4f,4.8f,4.8f,paper);
            rect(ui,px+std::sin(yaw)*5-1,pz-std::cos(yaw)*5-1,2,2,mint);
            if(alert>0&&distance2D(player,teacher)<15) rect(ui,mx+teacher.x/CELL*sz-2,my+teacher.z/CELL*sz-2,4,4,red);
            text(ui,mx,my+171,"GOLD: REPORT",1.35f,amber);text(ui,mx,my+190,"GREEN: EXIT",1.35f,mint);
        }
        if(alert>0) {
            float pulse=0.65f+0.35f*std::sin(gameTime*8);
            rect(ui,0,0,UIW,5,red*pulse);rect(ui,0,UIH-5,UIW,5,red*pulse);
            rect(ui,434,27,410,45,ink);text(ui,453,42,"THE PROFESSOR IS NEAR!",2.5f,red);
        }
        if(notice>0) {
            float w=float(noticeText.size())*6*1.8f;
            rect(ui,(UIW-w)/2-20,623,w+40,43,ink);text(ui,(UIW-w)/2,639,noticeText,1.8f,collected==5?mint:amber);
        } else if(collected<5 && distance2D(player,center(exitTile))<2) {
            text(ui,434,639,"EXIT REQUIRES 5 REPORTS",2,amber);
        }
    } else {
        rect(ui,91,159,1098,484,ink);rect(ui,91,159,6,484,mint);
        text(ui,128,191,"MOSCOW STATE UNIVERSITY  /  AFTER HOURS",2,muted);
        if(mode==Mode::Title) {
            text(ui,127,239,"ESCAPE FROM",6,paper);text(ui,127,303,"BIOPHYSICS",7,mint);
            text(ui,131,382,"THE LAST LAB IS OVER. THE PROFESSOR DISAGREES.",2,amber);
            text(ui,131,422,"RECOVER 5 LAB REPORTS. GET BACK TO THE GREEN EXIT.\nSTAY OUT OF SIGHT. E TO HIDE IN A LOCKER.",2,paper);
            text(ui,131,485,"WASD MOVE    MOUSE LOOK    SHIFT SPRINT\nF FLASHLIGHT    M MAP    F11 FULLSCREEN",2,muted);
            text(ui,131,567,"> ENTER TO BEGIN",3,mint);
            text(ui,814,578,"OPENGL 3.3 / C++17",1.7f,muted);
        } else if(mode==Mode::Paused) {
            text(ui,128,256,"TAKE A BREATH",6,paper);
            text(ui,131,340,"THE CORRIDORS CAN WAIT.",2.5f,muted);
            text(ui,131,412,"WASD MOVE   MOUSE / ARROWS LOOK   SHIFT SPRINT\nF FLASHLIGHT   M MAP   F11 FULLSCREEN",2,paper);
            text(ui,131,507,"> ENTER / ESC TO RESUME",3,mint);
            text(ui,131,568,"R GENERATES A NEW FLOOR",2,amber);
        } else {
            bool won=mode==Mode::Won;
            text(ui,128,257,won?"CREDIT RECEIVED!":"RETAKE REQUIRED",5.5f,won?mint:red);
            text(ui,131,333,won?"FIVE REPORTS. ONE EXIT. FREEDOM.":"LET US DERIVE THE NERNST EQUATION AGAIN.",2.5f,paper);
            text(ui,131,405,"TIME: "+elapsed()+"     REPORTS: "+std::to_string(collected)+" / 5",3,amber);
            text(ui,131,501,"> ENTER TO RETRY THIS FLOOR",2.8f,mint);
            text(ui,131,553,"R NEW FLOOR     ESC QUIT",2,muted);
        }
    }
    glDisable(GL_DEPTH_TEST);glUniform1i(locUI,1);Mat m=identity();glUniformMatrix4fv(locMatrix,1,GL_FALSE,m.m);drawMesh(ui);
}
#include "reports.h"

void draw() {
    beginSceneBuffer(screenW,screenH);
    glViewport(0,0,screenW,screenH);glClearColor(0.019f,0.035f,0.049f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glUniform1i(locUI,0);glUniform1i(locSkinEnabled,0);glUniform3f(locSkinOffset,0,0,0);
    bool moving=hiddenLocker<0&&mode==Mode::Playing&&(keys['W']||keys['A']||keys['S']||keys['D']);
    V3 eye=player+V3{0,1.66f+(moving?std::sin(footPhase)*0.027f:0),0};V3 dir=forward();
    Mat projection=perspective(72*PI/180,float(screenW)/screenH),view=lookAt(eye,dir);
    Mat m=projection*view;
    glUniformMatrix4fv(locView,1,GL_FALSE,view.m);
    glUniformMatrix4fv(locMatrix,1,GL_FALSE,m.m);glUniform3f(locEye,eye.x,eye.y,eye.z);glUniform3f(locForward,dir.x,dir.y,dir.z);
    glUniform3f(locTeacher,teacher.x,teacher.y,teacher.z);
    auto lights=ceilingLights;std::sort(lights.begin(),lights.end(),[&](V3 a,V3 b){return dot(a-eye,a-eye)<dot(b-eye,b-eye);});
    int lightCount=std::min(12,int(lights.size()));glUniform1i(locLightCount,lightCount);
    for(int i=0;i<lightCount;++i)glUniform3f(locLights[i],lights[i].x,lights[i].y,lights[i].z);
    glUniform1f(locLamp,lamp&&hiddenLocker<0?1.0f:0.0f);
    glBindVertexArray(worldVao);glDrawArrays(GL_TRIANGLES,0,GLsizei(staticMesh.size()));
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    static Mesh dynamic;dynamic.clear();dynamic.reserve(500000);
    for(const auto& l:lockers)placeKit(dynamic,lockerKit(l.door),l.tile,l.wall);
    for(size_t i=0;i<notes.size();++i) if(!notes[i].taken) {
        placeKit(dynamic,laboratoryReport(i),notes[i].tile,noteWall(notes[i].tile));
    }
    V3 toTeacher=teacher+V3{0,1,0}-eye;
    bool professorVisible=dot(toTeacher,dir)>-2.0f;
    if(runtimeProfessor.loaded) {
        drawMesh(dynamic);
        if(professorVisible) drawSkinnedRuntimeProfessor(teacher,professorAnimation);
    } else {
        if(professorVisible) animateProfessor(dynamic,teacher,professorAnimation);
        drawMesh(dynamic);
    }
    compositeSSAO(projection);
    glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glUniform1i(locSkinEnabled,0);glUniform3f(locSkinOffset,0,0,0);drawUI();
}
void screenshot(const std::string& path) {
    // Uncompressed BGR TGA; OpenGL's bottom-up rows match TGA's default origin.
    std::vector<uint8_t> pixels(size_t(screenW)*screenH*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,screenW,screenH,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    for(size_t i=0;i<pixels.size();i+=3) std::swap(pixels[i],pixels[i+2]);
    uint8_t header[18]{};header[2]=2;header[12]=uint8_t(screenW&255);header[13]=uint8_t(screenW>>8);
    header[14]=uint8_t(screenH&255);header[15]=uint8_t(screenH>>8);header[16]=24;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(header),18);out.write(reinterpret_cast<char*>(pixels.data()),std::streamsize(pixels.size()));
    if(!out) throw std::runtime_error("Could not write screenshot.");
}
void require(bool condition,const char* what) { if(!condition) throw std::runtime_error(what); }
void selfTest() {
    std::ofstream log("self-test.log");
    for(uint32_t s=1;s<=80;++s) {
        makeMap(s);require(notes.size()==5,"Expected five reports.");require(canStand(player),"Spawn collision.");
        require(route(tile(player),tile(teacher)).size()>10,"Professor spawns too close.");
        for(size_t i=0;i<notes.size();++i) {
            require(!route(tile(player),notes[i].tile).empty(),"Unreachable report.");
            for(size_t j=i+1;j<notes.size();++j) require(!(notes[i].tile==notes[j].tile),"Duplicate report.");
        }
        for(Tile p:openTiles) if(!(p==exitTile)) require(!route(exitTile,p).empty(),"Disconnected floor.");
        V3 start=player;movePlayer({-100,0,0});require(canStand(player)&&player.x>=CELL,"Wall tunnelling.");player=start;
        movePlayer({0,0,-100});require(canStand(player)&&player.z>=CELL,"Wall tunnelling.");
        if(s<=8){
            buildWorld(false);
            int expected=0,actual=0;
            for(Tile t:openTiles)for(Tile d:dirs)if(wallBoundary(t,d))++expected;
            std::map<std::array<int,4>,int> coverage;
            for(const WallRun& run:wallRuns)for(int j=0;j<run.count;++j){++actual;++coverage[{run.start.x+j*run.along.x,run.start.z+j*run.along.z,run.out.x,run.out.z}];}
            require(expected==actual,"Wall contour has holes.");for(const auto& count:coverage)require(count.second==1,"Duplicate wall face.");
            std::map<std::pair<int,int>,int> corners;
            for(const auto& run:wallRuns)for(bool end:{false,true}){V3 point=wallEndpoint(run,end,3.7f,0.11f);++corners[{int(std::round(point.x*1000)),int(std::round(point.z*1000))}];}
            for(const auto& corner:corners)require(corner.second==2,"Mitered cornice has an unpaired corner.");
            std::map<std::array<int,4>,int> decor;
            for(const auto& item:wallDecorations){
                require(item.kind!=3,"Pilaster still present.");
                require(++decor[{item.tile.x,item.tile.z,item.direction.x,item.direction.z}]==1,"Overlapping wall decorations.");
                for(const auto& note:notes)require(!(note.tile==item.tile&&noteWall(note.tile)==item.direction),"Report overlaps decoration.");
            }
        }
    }
    log<<"PASS: 80 seeds, report uniqueness, reachability, safe spawns, wall collisions.\n";
    makeMap(2026);mode=Mode::Playing;gameTime=0;collected=0;alert=0;aiTick=0;
    for(const Note& n:notes) { player=center(n.tile);update(0); }
    require(collected==5,"Reports are not collected on contact.");
    player=center(exitTile);update(0);require(mode==Mode::Won,"Exit did not trigger victory.");
    makeMap(2026);mode=Mode::Playing;collected=0;gameTime=7;teacher=player;update(0);
    require(mode==Mode::Lost,"Professor contact did not trigger defeat.");
    // Regression: off-center players in the same tile must still be caught.
    for(float dx:{-1.15f,0.0f,1.15f})for(float dz:{-1.15f,0.0f,1.15f}){
        makeMap(2026);mode=Mode::Playing;gameTime=7;alert=0;aiTick=0;collected=0;teacherPath.clear();
        teacher=center({9,9});player=teacher+V3{dx,0,dz};
        professorAnimation={};professorAnimation.facing=std::atan2(-dx,-dz);
        for(int frame=0;frame<120&&mode==Mode::Playing;++frame)update(1.0f/60);
        require(mode==Mode::Lost,"Professor failed to catch an off-center player.");
    }
    makeMap(2026);V3 c=center({9,9});
    require(caughtDuringStep(c+V3{-1,0,0},c+V3{1,0,0},c+V3{1,0,0},c+V3{-1,0,0}),"Crossing contact was skipped.");
    require(!caughtDuringStep(c,c,c+V3{1,0,0},c+V3{1,0,0}),"Capture radius too large.");
    bool testedCorner=false;
    for(int z=2;z<N-1&&!testedCorner;++z)for(int x=2;x<N-1&&!testedCorner;++x){
        if(walkable(x,z)||!walkable(x-1,z)||!walkable(x,z-1)||!walkable(x-1,z-1))continue;
        V3 p{x*CELL-.26f,0,z*CELL+.30f},q{x*CELL+.30f,0,z*CELL-.26f};
        require(canStand(p)&&canStand(q),"Invalid occlusion test positions.");
        require(!caughtDuringStep(p,p,q,q),"Professor caught player through a wall corner.");testedCorner=true;
    }
    require(testedCorner,"Missing wall corner regression fixture.");
    makeMap(2026);mode=Mode::Playing;gameTime=0;teacher=player;update(0);
    require(mode==Mode::Playing,"Initial grace period missing.");
    log<<"PASS: same-cell capture at nine offsets, swept contact, capture radius, wall occlusion, starting grace.\n";
    makeMap(2026);mode=Mode::Playing;gameTime=0;stamina=0.01f;exhausted=false;
    keys['W']=keys[VK_SHIFT]=true;update(0.1f);update(0.1f);
    require(exhausted&&!sprinting,"Sprint exhaustion did not stop sprinting.");
    keys['W']=keys[VK_SHIFT]=false;stamina=0.8f;update(0.1f);
    require(!exhausted&&stamina>0.8f,"Stamina did not recover.");
    mode=Mode::Paused;float before=gameTime;update(0.5f);require(gameTime==before,"Pause advances simulation.");
    makeMap(2026);mode=Mode::Playing;gameTime=7;alert=0;aiTick=0;teacherPath.clear();target=tile(teacher);
    for(int frame=0;frame<2400 && mode==Mode::Playing;++frame) {
        update(1.0f/60);require(canStand(teacher),"Professor clipped through a corner.");
    }
    log<<"PASS: pickups, victory, defeat, sprint exhaustion/recovery, pause, 40 seconds of AI navigation.\n";
    buildProfessor();size_t total=0;
    for(const BodyPart& part:professorParts)for(const Vertex& v:part.mesh) {
        require(std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z),"Invalid model vertex.");
        require(std::abs(length(v.n)-1)<0.001f,"Invalid model normal.");++total;
    }
    require(total>60000,"Detailed professor mesh was not built.");
    log<<"PASS: professor model, "<<total/3<<" triangles, finite vertices and unit normals.\n";
    for(int frame=0;frame<48;++frame){
        ProfessorAnimation a;a.blend=1;a.phase=frame*2*PI/48;a.time=3.81f+frame*0.003f;
        for(int side=0;side<2;++side){auto leg=legPose(a,side);require(std::abs(length(leg.knee-leg.hip)-0.4f)<0.002f,"Thigh IK length changed.");require(std::abs(length(leg.ankle-leg.knee)-0.35f)<0.002f,"Shin IK length changed.");require(leg.ankle.y>=0.109f,"Foot penetrates floor.");}
        if(frame%8==0){Mesh pose;animateProfessor(pose,{},a);for(const auto& v:pose){require(std::isfinite(v.p.y)&&std::isfinite(v.n.x),"Invalid animation vertex.");require(v.p.y>=-0.002f,"Animated shoe penetrates floor.");}}
    }
    ProfessorAnimation idle;advanceProfessor(idle,{},{},{1,0,0},1,false);require(idle.phase==0,"Idle animation advances the gait.");
    ProfessorAnimation moving;advanceProfessor(moving,{},{0,0,-ProfessorWalkStride*.5f},{0,0,-3},.5f,true);
    require(std::abs(moving.phase-PI)<0.001f,"Gait is not distance synchronized.");
    ProfessorAnimation chasing;advanceProfessor(chasing,{},{0,0,-3.15f},{0,0,-4.1f},1,true);
    require(chasing.runBlend>.99f&&chasing.reach>.9f,"Chase does not blend into running and reaching.");
    advanceProfessor(chasing,{},{},{0,0,-.8f},1,true,false);require(chasing.reach<.001f,"Professor reaches through an occluding wall.");
    SkinMatrix a{{1,0,0,0,0,1,0,0,0,0,1,0}},b{{1,0,0,0,0,-1,0,0,0,0,-1,0}};
    SkinMatrix half=blendSkin(a,b,.5f);require(std::abs(length(half.apply({0,1,0}))-1)<.0001f,"Rotation interpolation compresses limbs.");
    RuntimeClip reaching;reaching.frames=2;reaching.matrices={a,b};
    auto held=sampleSkin(reaching,10,0,1,false);require(std::abs(held.m[5]+1)<.0001f,"Reach restarts instead of holding its final pose.");
    log<<"PASS: wall contour coverage, exclusive decorations, report clearance, 48 IK poses, grounded feet, stationary gait.\n";
    makeMap(2026);require(!lockers.empty(),"No hiding places generated.");
    for(const auto& l:lockers){require(canStand(lockerApproach(l)),"Locker entrance is blocked.");require(!canStand(lockerPosition(l)),"Locker has no collision.");}
    auto setupLocker=[&](bool spotted){
        makeMap(2026);mode=Mode::Playing;gameTime=7;alert=0;aiTick=99;teacherPath.clear();professorAnimation={};
        auto& l=lockers.front();V3 out=lockerOut(l);player=lockerApproach(l);teacher=center(l.tile)-out*.75f;
        yaw=std::atan2(out.x,-out.z);pitch=0;
        professorAnimation.facing=spotted?std::atan2(-out.x,-out.z):std::atan2(out.x,out.z);
    };
    setupLocker(false);require(!professorSees(player),"Professor sees behind his back.");
    professorAnimation.facing+=PI;require(professorSees(player),"Professor misses a visible player.");
    setupLocker(false);gameTime=0;keys['W']=keys[VK_SHIFT]=true;stamina=1;exhausted=false;update(.05f);
    keys['W']=keys[VK_SHIFT]=false;require(alert==0,"Sprinting starts pursuit without visual detection.");
    setupLocker(false);useLocker();require(hiddenLocker==0&&knownLocker==-1,"Unseen hiding was revealed.");
    V3 hiddenPosition=player;teacher=lockerApproach(lockers[0]);keys['W']=true;
    for(int i=0;i<40;++i)update(.05f);
    keys['W']=false;require(mode==Mode::Playing&&alert==0&&distance2D(player,hiddenPosition)<.001f,"Safe hiding fails.");
    useLocker();require(hiddenLocker==-1&&canStand(player),"Cannot leave locker safely.");
    setupLocker(true);useLocker();require(knownLocker==0,"Witnessed hiding was forgotten.");
    teacher=lockerApproach(lockers[0]);for(int i=0;i<10;++i)update(.05f);
    require(mode==Mode::Playing&&lockerSearch>.4f,"Caught before opening locker.");
    mode=Mode::Paused;float searchBefore=lockerSearch;update(.5f);require(lockerSearch==searchBefore,"Paused locker opening advances.");mode=Mode::Playing;
    for(int i=0;i<35&&mode==Mode::Playing;++i)update(.05f);
    require(mode==Mode::Lost&&lockers[0].door>.9f,"Professor does not open witnessed locker before capture.");
    for(bool behindWall:{false,true}){
        setupLocker(false);alert=4;
        if(behindWall){
            bool found=false;for(Tile t:openTiles)if(!lineOfSight(center(t),player)){
                teacher=center(t);V3 delta=player-teacher;professorAnimation.facing=std::atan2(-delta.x,-delta.z);found=true;break;
            }
            require(found,"Missing occluded locker fixture.");
        }
        target=tile(teacher);Tile remembered=target;
        require(!professorSees(player),"Unseen entry fixture is visible.");
        useLocker();require(hiddenLocker==0&&knownLocker==-1&&target==remembered&&alert==4,"Pursuit reveals an unseen locker entry.");
        teacher=lockerApproach(lockers[0]);
        for(int i=0;i<40;++i)update(.05f);
        require(mode==Mode::Playing&&knownLocker==-1&&lockerSearch==0&&lockers[0].door<.01f,"Professor opens an unwitnessed locker.");
    }
    setupLocker(true);alert=4;useLocker();require(knownLocker==0,"Witnessed entry during pursuit was forgotten.");
    teacher=lockerApproach(lockers[0]);update(.2f);useLocker();player=center({8,9});
    for(int i=0;i<45&&mode==Mode::Playing;++i)update(.05f);
    require(mode==Mode::Playing&&knownLocker==-1,"Empty locker captures escaped player.");
    makeMap(2026);require(hiddenLocker==-1&&knownLocker==-1&&lockerSearch==0,"Restart retains hiding state.");
    log<<"PASS: vision cone, locker collision, unseen hiding, exit, witnessed search, opening before capture, pause and reset.\n";
    log<<"PASS: active pursuit does not reveal locker entry behind a wall or outside the vision cone.\n";
    log.flush();
}
void exportProfessor() {
    std::ofstream obj("professor.obj"),mtl("professor.mtl");obj<<"mtllib professor.mtl\n";
    std::map<std::string,V3> colors;size_t vertexBase=1;
    auto materialName=[](const Vertex& v){return "surface_"+std::to_string(int(v.material))+"_"+std::to_string(int(v.c.x*1000))+"_"+std::to_string(int(v.c.y*1000))+"_"+std::to_string(int(v.c.z*1000));};
    for(size_t part=0;part<professorParts.size();++part) {
        const auto& mesh=professorParts[part].mesh;obj<<"o Professor_part_"<<part<<"\n";
        for(const Vertex& v:mesh)obj<<"v "<<v.p.x<<' '<<-v.p.z<<' '<<v.p.y<<'\n';
        for(const Vertex& v:mesh)obj<<"vn "<<v.n.x<<' '<<-v.n.z<<' '<<v.n.y<<'\n';
        std::string previous;
        for(size_t i=0;i<mesh.size();i+=3) {
            auto name=materialName(mesh[i]);colors[name]=mesh[i].c;
            if(name!=previous){obj<<"usemtl "<<name<<'\n';previous=name;}
            obj<<"f";for(size_t j=0;j<3;++j){size_t k=vertexBase+i+j;obj<<' '<<k<<"//"<<k;}obj<<'\n';
        }
        vertexBase+=mesh.size();
    }
    for(const auto& pair:colors)mtl<<"newmtl "<<pair.first<<"\nKd "<<pair.second.x<<' '<<pair.second.y<<' '<<pair.second.z<<"\nKs 0.04 0.04 0.04\nNs 30\n\n";
    if(!obj||!mtl)throw std::runtime_error("Could not export professor OBJ/MTL.");
}
void shutdown() {
    captureMouse(false);
    if(renderContext) {
        shutdownSSAO();
        glDeleteTextures(3,materialTextures);
        if(emblemTexture)glDeleteTextures(1,&emblemTexture);
        if(reportTexture)glDeleteTextures(1,&reportTexture);
        if(posterTexture)glDeleteTextures(1,&posterTexture);
        if(runtimeVbo)glDeleteBuffers(1,&runtimeVbo);if(runtimeVao)glDeleteVertexArrays(1,&runtimeVao);
        if(worldVbo)glDeleteBuffers(1,&worldVbo);if(worldVao)glDeleteVertexArrays(1,&worldVao);
        if(vbo) glDeleteBuffers(1,&vbo);if(vao) glDeleteVertexArrays(1,&vao);if(program) glDeleteProgram(program);
        wglMakeCurrent(nullptr,nullptr);wglDeleteContext(renderContext);renderContext=nullptr;
    }
    if(deviceContext&&windowHandle) ReleaseDC(windowHandle,deviceContext);
    if(windowHandle) DestroyWindow(windowHandle);
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR args,int) {
    SetProcessDPIAware();
    std::wstring cmd=args?args:L"";
    bool test=cmd.find(L"--self-test")!=std::wstring::npos;
    bool smoke=cmd.find(L"--smoke-test")!=std::wstring::npos;
    ssaoEnabled=cmd.find(L"--no-ssao")==std::wstring::npos;
    try {
        if(test) { selfTest();return 0; }
        if(cmd.find(L"--export-model")!=std::wstring::npos){buildProfessor();exportProfessor();return 0;}
        createWindowAndGL(smoke);buildProfessor();
        require(loadRuntimeProfessor(),"Rigged professor asset missing: assets/models/teacher_runtime.bin");
        initRenderer();
        makeMap(currentSeed);buildWorld();
        yaw=walkable(2,1)?PI/2:PI;
        if(smoke) {
            GLint major=0,minor=0,profile=0;
            glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);glGetIntegerv(GL_CONTEXT_PROFILE_MASK,&profile);
            require((major>3||(major==3&&minor>=3))&&(profile&GL_CONTEXT_CORE_PROFILE_BIT),"Expected OpenGL 3.3+ Core.");
            draw();screenshot("title.tga");mode=Mode::Playing;gameTime=12;update(0);draw();screenshot("gameplay.tga");
            // A deterministic close-up also exercises the pursuer and report geometry.
            Tile report=notes.front().tile;
            for(Tile direction:dirs) if(walkable(report.x+direction.x,report.z+direction.z)) {
                player=center({report.x+direction.x,report.z+direction.z});
                teacher=center(report);V3 facing=normalized(teacher-player);teacher=teacher-facing*0.9f;
                yaw=std::atan2(facing.x,-facing.z);pitch=-0.07f;alert=7;
                professorAnimation.facing=std::atan2(facing.x,facing.z);
                update(0);draw();screenshot("pursuit.tga");break;
            }
            player=teacher+V3{0,0,-0.83f};yaw=PI;pitch=0.02f;professorAnimation.facing=0;draw();screenshot("portrait.tga");
            teacher=center({4,3});player=center({3,3});yaw=PI/2;pitch=-0.08f;professorAnimation.facing=PI/2;
            for(int frame=0;frame<12;++frame){professorAnimation.blend=1;professorAnimation.phase=frame*2*PI/12;professorAnimation.time=frame*0.1f;draw();screenshot("walk-"+std::to_string(frame)+".tga");}
            professorAnimation.blend=0;
            professorAnimation.reach=1;teacher=center({4,3});player=center({3,3});yaw=PI/2;pitch=-.10f;
            draw();screenshot("reach.tga");
            player=teacher+V3{-1.6f,0,-1.05f};yaw=std::atan2(1.6f,-1.05f);pitch=-.22f;
            draw();screenshot("reach-side.tga");professorAnimation.reach=0;
            player=center({2,3});yaw=PI/2;pitch=0.02f;alert=0;
            ssaoEnabled=false;draw();screenshot("corridor-no-ssao.tga");
            ssaoEnabled=true;draw();screenshot("corridor.tga");
            std::vector<float> aoValues(size_t(aoWidth)*aoHeight);
            glBindFramebuffer(Framebuffer,aoFbos[0]);glReadPixels(0,0,aoWidth,aoHeight,GL_RED,GL_FLOAT,aoValues.data());glBindFramebuffer(Framebuffer,0);
            float minAO=1,meanAO=0;for(float value:aoValues){require(std::isfinite(value)&&value>=0&&value<=1.001f,"Invalid SSAO output.");minAO=std::min(minAO,value);meanAO+=value;}
            meanAO/=float(aoValues.size());require(minAO<.92f&&meanAO>.35f,"SSAO is absent or over-darkened.");
            {const Note& n=notes.front();Tile d=noteWall(n.tile);player=center(n.tile)+V3{float(d.x)*.58f,0,float(d.z)*.58f};
             yaw=std::atan2(float(d.x),-float(d.z));pitch=-.09f;draw();screenshot("report.tga");}
            for(const auto& item:wallDecorations)if(item.kind==1){player=center(item.tile);yaw=std::atan2(float(item.direction.x),-float(item.direction.z));pitch=0.12f;draw();screenshot("poster.tga");break;}
            for(const auto& item:wallDecorations)if(item.kind==2){player=center(item.tile)-V3{float(item.direction.x),0,float(item.direction.z)}*.9f;yaw=std::atan2(float(item.direction.x),-float(item.direction.z));pitch=-.02f;draw();screenshot("door.tga");break;}
            player=center({8,9});yaw=PI/2;pitch=0.02f;draw();screenshot("hall.tga");
            require(!lockers.empty(),"Missing locker render fixture.");
            {auto& l=lockers.front();V3 out=lockerOut(l);
             player=center(l.tile)-out*.65f;teacher=center({9,9});yaw=std::atan2(out.x,-out.z);pitch=-.25f;alert=0;
             draw();screenshot("locker.tga");
             player=lockerApproach(l);useLocker();l.door=0;draw();screenshot("locker-hidden.tga");
             knownLocker=hiddenLocker;teacher=lockerApproach(l);professorAnimation.facing=std::atan2(-out.x,-out.z);teacherPath.clear();
             for(int i=0;i<28;++i)update(.05f);
             require(mode==Mode::Playing&&l.door>.6f,"Locker opening render did not advance.");
             draw();screenshot("locker-opening.tga");
             hiddenLocker=knownLocker=-1;lockerSearch=0;alert=0;notice=0;l.door=0;
             player=center({8,9});teacher=center({4,3});yaw=PI/2;pitch=.02f;}
            require(glGetError()==GL_NO_ERROR,"OpenGL reported an error.");
            std::ofstream log("smoke-test.log");log<<"PASS: shaders, buffers, world, HUD, screenshots; OpenGL "<<glGetString(GL_VERSION)<<"\nGPU: "<<glGetString(GL_RENDERER)<<"\n";
            log<<"SSAO: min="<<minAO<<", mean="<<meanAO<<", half resolution, bilateral blur, ambient-only composite\n";
            {int w=screenW,h=screenH;screenW=1001;screenH=633;draw();require(postWidth==1001&&aoWidth==501,"SSAO resize failed.");
             screenW=1;screenH=1;draw();require(aoWidth==1&&aoHeight==1,"SSAO minimized size failed.");
             screenW=w;screenH=h;draw();require(glGetError()==GL_NO_ERROR,"SSAO resize produced GL errors.");}
            glFinish();auto start=std::chrono::steady_clock::now();
            for(int i=0;i<12;++i){draw();glFinish();}
            log<<"Mean render time, 1280x800, 12 frames: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/12<<" ms\n";
            teacher=center({4,3});player=center({3,3});yaw=PI/2;pitch=-0.08f;professorAnimation.facing=PI/2;professorAnimation.blend=1;
            glFinish();start=std::chrono::steady_clock::now();
            for(int i=0;i<12;++i){professorAnimation.phase=i*PI/6;draw();glFinish();}
            log<<"Animated professor in view, 12 frames: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/12<<" ms\n";
            shutdown();return 0;
        }
        auto previous=std::chrono::steady_clock::now();
        while(running) {
            MSG msg;
            while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)) { if(msg.message==WM_QUIT) running=false;TranslateMessage(&msg);DispatchMessage(&msg); }
            auto now=std::chrono::steady_clock::now();float dt=std::min(0.05f,std::chrono::duration<float>(now-previous).count());previous=now;
            if(!focused||IsIconic(windowHandle)) { Sleep(20);std::fill(std::begin(pressed),std::end(pressed),false);continue; }
            update(dt);std::fill(std::begin(pressed),std::end(pressed),false);draw();SwapBuffers(deviceContext);Sleep(1);
        }
        shutdown();return 0;
    } catch(const std::exception& e) {
        std::ofstream log("error.log");log<<e.what()<<'\n';log.close();
        if(!smoke&&!test) MessageBoxA(windowHandle,e.what(),"Escape from Biophysics - Error",MB_OK|MB_ICONERROR);
        shutdown();return 1;
    }
}
