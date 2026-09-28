#pragma once
// Procedural material graph: coordinates -> noise/Voronoi -> layers -> bump -> BRDF.
const char* materialShader=R"GLSL(#version 330 core
in vec3 world; in vec3 normal; in vec3 color; in vec3 tex;
flat in int material;
uniform vec3 uEye; uniform vec3 uForward;
uniform vec3 uTeacher;
uniform sampler2DArray uAlbedoMaps,uNormalMaps,uRoughMaps;
uniform sampler2D uEmblem;
uniform vec3 uLights[12]; uniform int uLightCount;
uniform int uUI; uniform float uLamp;
out vec4 frag;
float hash(vec3 p){p=fract(p*0.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float noise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),
mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z);}
float fbm(vec3 p){return noise(p)*0.57+noise(p*2.07)*0.28+noise(p*4.13)*0.15;}
vec3 bump(vec3 n,float h,float strength){
 vec3 px=dFdx(world),py=dFdy(world),r1=cross(py,n),r2=cross(n,px);
 float det=dot(px,r1);vec3 grad=sign(det)*(dFdx(h)*r1+dFdy(h)*r2);
 return normalize(abs(det)*n-strength*grad);
}
void main(){
 if(uUI==1){frag=vec4(color,1);return;}
 vec3 n=normalize(normal),albedo=color,p=tex;
 float rough=0.72,metal=0.,height=0.,ao=1.;
 float grain=noise(p*180.),coarse=fbm(p*3.1);
 if(material==1||material==2){
   float stains=fbm(p*vec3(1.7,0.22,1.7));
   float flecks=smoothstep(0.68,0.79,noise(p*29.));
   albedo*=0.91+0.15*coarse+0.055*grain;
   albedo=mix(albedo,vec3(0.32,0.29,0.23),flecks*0.08);
   albedo*=1.-0.13*smoothstep(0.55,0.85,stains);
   ao=0.79+0.21*smoothstep(0.08,0.55,world.y);
   height=grain*0.045+noise(p*55.)*0.06;rough=material==1?0.44:0.87;
 }
 if(material==3){
   vec2 cell=p.xz/0.8,edge=min(fract(cell),1.-fract(cell));
   float aa=max(fwidth(cell.x),fwidth(cell.y));
   float grout=1.-smoothstep(0.005,0.012+aa,min(edge.x,edge.y));
   float tileNoise=hash(vec3(floor(cell),7.));
   float chips=noise(p*125.);float speck=smoothstep(0.61,0.75,chips);
   albedo=mix(vec3(0.43,0.40,0.34),vec3(0.64,0.60,0.51),tileNoise);
   albedo=mix(albedo,vec3(0.15,0.13,0.10),speck*0.60);
   albedo+=smoothstep(0.71,0.79,noise(p*87.+9.))*0.19;
   albedo*=0.89+0.17*coarse;
   vec2 band=abs(fract(p.xz/3.2)-0.5);
   if(max(band.x,band.y)>0.46)albedo*=vec3(0.43,0.37,0.33);
   albedo=mix(albedo,vec3(0.18,0.165,0.14),grout*0.8);
   height=grain*0.02-grout*0.15;rough=0.25+0.13*coarse;ao=1.-0.15*grout;
 }
 if(material==4){albedo*=0.92+0.1*grain;rough=0.93;height=grain*0.045;}
 if(material==5){
   float weave=sin(p.x*1700.)*sin(p.y*1700.)*0.5+0.5;
   float fade=1.-smoothstep(0.001,0.005,length(fwidth(p)));
   albedo*=0.91+0.10*coarse+0.035*weave*fade;height=(weave*0.012*fade+grain*0.015);rough=0.85;
 }
 if(material==6||material==14){
   albedo*=0.94+0.1*noise(p*96.);albedo+=vec3(0.027,-0.005,-0.006)*(coarse-0.3);
   height=grain*0.007;rough=0.52;
 }
 if(material==13){rough=0.18;}
 if(material==7){rough=0.25;metal=0.78;albedo*=0.92+grain*0.08;}
 if(material==8){rough=0.7;albedo*=0.9+grain*0.2;}
 if(material==9){albedo*=0.86+0.2*grain;height=grain*0.024;rough=0.36;}
 if(material==10){
   float g=fbm(p*vec3(35.,1.2,35.));
   float veins=sin(p.y*1.7+g*25.+noise(p*2.)*4.);
   albedo*=0.64+0.38*g+0.10*veins;height=g*0.035;rough=0.34;
 }
 if(material==11){
   float vein=pow(1.-abs(sin(dot(p,vec3(1.3,0.7,0.6))*11.+fbm(p*3.8)*15.)),22.);
   albedo*=0.88+0.20*coarse;albedo=mix(albedo,albedo*0.64,vein*0.30);
   height=noise(p*55.)*0.008;rough=0.25;
 }
 if(material==15){rough=0.9;height=0.;}
 if(material==12){albedo=texture(uEmblem,p.xy).rgb;rough=0.9;height=0.;}
 n=bump(n,height,0.003);
 if(material==1||material==2||material==3||material==10){
   vec3 a=abs(normal);vec2 uv=a.y>0.7?p.xz:(a.x>a.z?p.zy:p.xy);
   int layer=material==3?1:(material==10?2:0);
   uv*=material==3?0.625:(material==10?0.7:0.5);
   vec3 coord=vec3(uv,float(layer));
   vec3 photo=texture(uAlbedoMaps,coord).rgb;
   if(layer==0)albedo=photo*vec3(0.91,0.91,0.88);
   if(layer==1){
     albedo=photo*vec3(0.83,0.80,0.72);

   }
   if(layer==2)albedo=photo*color*2.5;
   rough=clamp(texture(uRoughMaps,coord).r,0.22,0.93);
   vec3 mapped=texture(uNormalMaps,coord).xyz*2.-1.;
   mapped.xy*=layer==0?0.23:0.5;
   vec3 dp1=dFdx(world),dp2=dFdy(world);vec2 st1=dFdx(uv),st2=dFdy(uv);
   vec3 T=normalize(dp1*st2.y-dp2*st1.y);
   vec3 B=normalize(-dp1*st2.x+dp2*st1.x);
   n=normalize(T*mapped.x+B*mapped.y+normalize(normal)*mapped.z);
 }
 vec3 delta=uEye-world;float dist=length(delta);vec3 V=normalize(delta);
 float cone=smoothstep(0.80,0.97,dot(-V,uForward));
 float flashlight=cone*uLamp*1.65/(1.+dist*dist*0.05);
 float expn=mix(150.,5.,rough*rough);
 vec3 F0=mix(vec3(0.035),albedo,metal);
 vec3 base=albedo*vec3(0.15,0.15,0.14)*ao;
 for(int i=0;i<uLightCount;++i){
   vec3 dl=uLights[i]-world;float ld=length(dl);vec3 L=dl/max(ld,0.001);
   float energy=0.9/(1.+ld*ld*0.65)*(1.-smoothstep(20.,28.,ld));
   float diffuse=max(dot(n,L),0.);
   vec3 H=normalize(L+V);
   vec3 spec=F0*pow(max(dot(n,H),0.),expn)*(expn+2.)/18.;
   base+=(albedo*diffuse+spec)*vec3(1.,0.93,0.80)*energy;
 }
 base+=albedo*vec3(1.,0.94,0.84)*max(dot(n,V),0.)*flashlight;
 base+=F0*pow(max(dot(n,V),0.),expn)*flashlight*0.4;
 // Soft contact shadow, kept local to the character's feet.
 if(material==3&&world.y<0.03){
   vec2 q=world.xz-uTeacher.xz;
   ao*=1.-0.58*exp(-dot(q,q)*5.5);
 }
 base*=ao;
 base=base/(vec3(1.)+base*0.55);
 if(max(color.r,max(color.g,color.b))>1.)base=color;
 float fog=1.-exp(-dist*dist*0.00050);
 base=mix(base,vec3(0.045,0.045,0.041),fog);
 frag=vec4(pow(max(base,vec3(0)),vec3(0.87)),1);
}
)GLSL";
