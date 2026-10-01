#pragma once
// Forward lighting with MRT ambient separation. SSAO affects indirect light;
// emissive lamps, the flashlight's direct light and the HUD stay readable.
constexpr GLenum Framebuffer=0x8D40,ColorAttachment=0x8CE0,DepthAttachment=0x8D00;
constexpr GLenum FramebufferComplete=0x8CD5,RGBA16F=0x881A,R16F=0x822D,Depth24=0x81A6;
GLuint sceneFbo=0,sceneTextures[4]{},aoFbos[2]{},aoTextures[2]{};
GLuint postVao=0,aoProgram=0,blurProgram=0,compositeProgram=0;
int postWidth=0,postHeight=0,aoWidth=0,aoHeight=0;
bool ssaoEnabled=true;
const char* fullscreenVertex=R"GLSL(#version 330 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0.,1.);}
)GLSL";
const char* ssaoFragment=R"GLSL(#version 330 core
in vec2 uv;out float occlusion;
uniform sampler2D uDepth,uNormals;
uniform mat4 uProjection;
uniform vec3 uKernel[24];
vec3 positionAt(vec2 q){float ndc=texture(uDepth,q).r*2.-1.;float z=-uProjection[3][2]/(ndc+uProjection[2][2]);return vec3(-z*(q*2.-1.)/vec2(uProjection[0][0],uProjection[1][1]),z);}
void main(){
 if(texture(uDepth,uv).r>=.999999){occlusion=1.;return;}
 vec3 p=positionAt(uv),n=normalize(texture(uNormals,uv).xyz);
 vec3 axis=abs(n.z)<.9?vec3(0,0,1):vec3(0,1,0);
 vec3 t=normalize(cross(axis,n)),b=cross(n,t);
 vec2 cell=mod(floor(gl_FragCoord.xy),4.);
 float angle=6.2831853*fract(sin(dot(cell,vec2(12.9898,78.233)))*43758.5453);
 mat3 basis=mat3(t*cos(angle)+b*sin(angle),-t*sin(angle)+b*cos(angle),n);
 float total=0.;const float radius=.52;const float bias=.018;
 for(int i=0;i<24;++i){
   vec3 q=p+basis*uKernel[i]*radius;
   vec4 projected=uProjection*vec4(q,1.);
   if(projected.w<=0.)continue;
   vec2 st=projected.xy/projected.w*.5+.5;
   if(any(lessThan(st,vec2(0)))||any(greaterThan(st,vec2(1)))||texture(uDepth,st).r>=.999999)continue;
   float z=positionAt(st).z;
   float weight=smoothstep(0.,1.,radius/max(abs(p.z-z),.001));
   total+=(z>=q.z+bias?1.:0.)*weight;
 }
 float fade=1.-smoothstep(18.,35.,-p.z);
 occlusion=clamp(1.-total/24.*1.65*fade,.30,1.);
}
)GLSL";
const char* ssaoBlurFragment=R"GLSL(#version 330 core
in vec2 uv;out float result;
uniform sampler2D uAO,uDepth,uNormals;
uniform mat4 uProjection;
uniform int uHorizontal;
float zAt(vec2 p){return -uProjection[3][2]/(texture(uDepth,p).r*2.-1.+uProjection[2][2]);}
void main(){
 if(texture(uDepth,uv).r>=.999999){result=1.;return;}
 vec3 n=normalize(texture(uNormals,uv).xyz);float z=zAt(uv),sum=0.,weight=0.;
 vec2 stepUV=(uHorizontal==1?vec2(1,0):vec2(0,1))/vec2(textureSize(uAO,0));
 for(int i=-2;i<=2;++i){vec2 q=uv+stepUV*float(i);
   float w=exp(-float(i*i)*.32)*exp(-abs(zAt(q)-z)*18./max(1.,-z*.08));
   w*=pow(max(dot(n,normalize(texture(uNormals,q).xyz)),0.),12.);
   sum+=texture(uAO,q).r*w;weight+=w;
 }
 result=weight>.0001?sum/weight:1.;
}
)GLSL";
const char* ssaoCompositeFragment=R"GLSL(#version 330 core
in vec2 uv;out vec4 frag;
uniform sampler2D uScene,uAmbient,uAO,uDepth,uNormals;
uniform mat4 uProjection;
uniform int uEnabled;
vec3 positionAt(vec2 q){float z=-uProjection[3][2]/(texture(uDepth,q).r*2.-1.+uProjection[2][2]);return vec3(-z*(q*2.-1.)/vec2(uProjection[0][0],uProjection[1][1]),z);}
void main(){
 if(texture(uDepth,uv).r>=.999999){frag=vec4(.019,.035,.049,1);return;}
 vec3 p=positionAt(uv),normal=normalize(texture(uNormals,uv).xyz);
 vec3 lit=texture(uScene,uv).rgb;vec4 ambient=texture(uAmbient,uv);
 float ao=1.;
 if(uEnabled==1){
   vec2 size=vec2(textureSize(uAO,0)),pixel=uv*size-.5,base=floor(pixel),f=fract(pixel);
   float value=0.,weights=0.;
   for(int y=0;y<2;++y)for(int x=0;x<2;++x){
     vec2 q=(base+vec2(x,y)+.5)/size;
     float w=(x==0?1.-f.x:f.x)*(y==0?1.-f.y:f.y);
     w*=exp(-abs(positionAt(q).z-p.z)*24./max(1.,-p.z*.06));
     w*=pow(max(dot(normal,normalize(texture(uNormals,q).xyz)),0.),8.);
     value+=texture(uAO,q).r*w;weights+=w;
   }
   ao=weights>.0001?value/weights:1.;
 }
 vec3 base=max(lit-ambient.rgb*(1.-ao),vec3(0));
 if(ambient.a<.5)base=base/(vec3(1)+base*.55);
 else base=lit;
 float fog=1.-exp(-dot(p,p)*.00050);
 base=mix(base,vec3(.045,.045,.041),fog);
 frag=vec4(pow(max(base,vec3(0)),vec3(.87)),1);
}
)GLSL";

GLuint postProgram(const char* fragment){
    GLuint vs=compileShader(GL_VERTEX_SHADER,fullscreenVertex),fs=compileShader(GL_FRAGMENT_SHADER,fragment);
    GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
    glDeleteShader(vs);glDeleteShader(fs);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[4096]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);throw std::runtime_error(log);}
    return p;
}
void bindPostTexture(int unit,GLuint texture){glActiveTexture(Texture0+unit);glBindTexture(GL_TEXTURE_2D,texture);}
void initSSAO(){
    glGenVertexArrays(1,&postVao);
    aoProgram=postProgram(ssaoFragment);blurProgram=postProgram(ssaoBlurFragment);compositeProgram=postProgram(ssaoCompositeFragment);
    for(GLuint p:{aoProgram,blurProgram,compositeProgram}){
        glUseProgram(p);
        for(const auto& binding:std::array<std::pair<const char*,int>,5>{{{"uScene",4},{"uNormals",5},{"uAmbient",6},{"uDepth",7},{"uAO",8}}})
            glUniform1i(glGetUniformLocation(p,binding.first),binding.second);
    }
    std::mt19937 random(20260930);std::uniform_real_distribution<float> random01(0,1);
    glUseProgram(aoProgram);
    for(int i=0;i<24;++i){
        V3 p=normalized({random01(random)*2-1,random01(random)*2-1,random01(random)});
        float t=float(i)/23,scale=(.10f+.90f*t*t)*(.65f+.35f*random01(random));p=p*scale;
        glUniform3f(glGetUniformLocation(aoProgram,("uKernel["+std::to_string(i)+"]").c_str()),p.x,p.y,p.z);
    }
}
void destroySceneBuffers(){
    glDeleteFramebuffers(1,&sceneFbo);glDeleteFramebuffers(2,aoFbos);
    glDeleteTextures(4,sceneTextures);glDeleteTextures(2,aoTextures);
    sceneFbo=0;std::fill(std::begin(aoFbos),std::end(aoFbos),0);
    std::fill(std::begin(sceneTextures),std::end(sceneTextures),0);std::fill(std::begin(aoTextures),std::end(aoTextures),0);
}
void allocatePostTexture(GLuint texture,GLint format,GLenum channels,int width,int height){
    bindPostTexture(4,texture);glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,channels,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
}
void checkPostFramebuffer(){if(glCheckFramebufferStatus(Framebuffer)!=FramebufferComplete)throw std::runtime_error("SSAO framebuffer is incomplete.");}
void beginSceneBuffer(int width,int height){
    if(width!=postWidth||height!=postHeight){
        destroySceneBuffers();postWidth=width;postHeight=height;aoWidth=(width+1)/2;aoHeight=(height+1)/2;
        glGenFramebuffers(1,&sceneFbo);glGenTextures(4,sceneTextures);glBindFramebuffer(Framebuffer,sceneFbo);
        for(int i=0;i<3;++i){allocatePostTexture(sceneTextures[i],RGBA16F,GL_RGBA,width,height);glFramebufferTexture2D(Framebuffer,ColorAttachment+i,GL_TEXTURE_2D,sceneTextures[i],0);}
        allocatePostTexture(sceneTextures[3],Depth24,GL_DEPTH_COMPONENT,width,height);glFramebufferTexture2D(Framebuffer,DepthAttachment,GL_TEXTURE_2D,sceneTextures[3],0);
        const GLenum outputs[]={ColorAttachment,ColorAttachment+1,ColorAttachment+2};glDrawBuffers(3,outputs);checkPostFramebuffer();
        glGenFramebuffers(2,aoFbos);glGenTextures(2,aoTextures);
        for(int i=0;i<2;++i){glBindFramebuffer(Framebuffer,aoFbos[i]);allocatePostTexture(aoTextures[i],R16F,GL_RED,aoWidth,aoHeight);
            glFramebufferTexture2D(Framebuffer,ColorAttachment,GL_TEXTURE_2D,aoTextures[i],0);glDrawBuffers(1,outputs);checkPostFramebuffer();}
    }
    glBindFramebuffer(Framebuffer,sceneFbo);glActiveTexture(Texture0);
}
void compositeSSAO(const Mat& projection){
    glDisable(GL_DEPTH_TEST);glBindVertexArray(postVao);
    for(int i=0;i<4;++i)bindPostTexture(4+i,sceneTextures[i]);
    if(ssaoEnabled){
        glViewport(0,0,aoWidth,aoHeight);glBindFramebuffer(Framebuffer,aoFbos[0]);glUseProgram(aoProgram);
        glUniformMatrix4fv(glGetUniformLocation(aoProgram,"uProjection"),1,GL_FALSE,projection.m);glDrawArrays(GL_TRIANGLES,0,3);
        glUseProgram(blurProgram);glUniformMatrix4fv(glGetUniformLocation(blurProgram,"uProjection"),1,GL_FALSE,projection.m);
        for(int i=0;i<2;++i){glBindFramebuffer(Framebuffer,aoFbos[1-i]);bindPostTexture(8,aoTextures[i]);
            glUniform1i(glGetUniformLocation(blurProgram,"uHorizontal"),i==0?1:0);glDrawArrays(GL_TRIANGLES,0,3);}
    }
    glBindFramebuffer(Framebuffer,0);glViewport(0,0,postWidth,postHeight);glUseProgram(compositeProgram);bindPostTexture(8,aoTextures[0]);
    glUniformMatrix4fv(glGetUniformLocation(compositeProgram,"uProjection"),1,GL_FALSE,projection.m);
    glUniform1i(glGetUniformLocation(compositeProgram,"uEnabled"),ssaoEnabled?1:0);glDrawArrays(GL_TRIANGLES,0,3);glActiveTexture(Texture0);
}
void shutdownSSAO(){
    if(glDeleteFramebuffers)destroySceneBuffers();
    if(postVao)glDeleteVertexArrays(1,&postVao);
    for(GLuint p:{aoProgram,blurProgram,compositeProgram})if(p)glDeleteProgram(p);
}
