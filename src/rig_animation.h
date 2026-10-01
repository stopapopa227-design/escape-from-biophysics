#pragma once
struct Rotation {float w=1,x=0,y=0,z=0;};
Rotation unitRotation(Rotation q){float r=1/std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);return {q.w*r,q.x*r,q.y*r,q.z*r};}
Rotation skinRotation(const SkinMatrix& m){
    float trace=m.m[0]+m.m[5]+m.m[10];Rotation q;
    if(trace>0){float s=std::sqrt(trace+1)*2;q={s*.25f,(m.m[9]-m.m[6])/s,(m.m[2]-m.m[8])/s,(m.m[4]-m.m[1])/s};}
    else if(m.m[0]>m.m[5]&&m.m[0]>m.m[10]){float s=std::sqrt(1+m.m[0]-m.m[5]-m.m[10])*2;q={(m.m[9]-m.m[6])/s,s*.25f,(m.m[1]+m.m[4])/s,(m.m[2]+m.m[8])/s};}
    else if(m.m[5]>m.m[10]){float s=std::sqrt(1+m.m[5]-m.m[0]-m.m[10])*2;q={(m.m[2]-m.m[8])/s,(m.m[1]+m.m[4])/s,s*.25f,(m.m[6]+m.m[9])/s};}
    else{float s=std::sqrt(1+m.m[10]-m.m[0]-m.m[5])*2;q={(m.m[4]-m.m[1])/s,(m.m[2]+m.m[8])/s,(m.m[6]+m.m[9])/s,s*.25f};}
    return unitRotation(q);
}
SkinMatrix blendSkin(const SkinMatrix& a,const SkinMatrix& b,float t){
    Rotation x=skinRotation(a),y=skinRotation(b);float d=x.w*y.w+x.x*y.x+x.y*y.y+x.z*y.z;
    if(d<0){y={-y.w,-y.x,-y.y,-y.z};d=-d;}
    float u=1-t,v=t;
    if(d<.9995f){float theta=std::acos(std::clamp(d,0.f,1.f)),s=std::sin(theta);u=std::sin((1-t)*theta)/s;v=std::sin(t*theta)/s;}
    Rotation q=unitRotation({x.w*u+y.w*v,x.x*u+y.x*v,x.y*u+y.y*v,x.z*u+y.z*v});
    SkinMatrix result{{1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y-q.z*q.w),2*(q.x*q.z+q.y*q.w),a.m[3]+(b.m[3]-a.m[3])*t,
                       2*(q.x*q.y+q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z-q.x*q.w),a.m[7]+(b.m[7]-a.m[7])*t,
                       2*(q.x*q.z-q.y*q.w),2*(q.y*q.z+q.x*q.w),1-2*(q.x*q.x+q.y*q.y),a.m[11]+(b.m[11]-a.m[11])*t}};
    return result;
}
SkinMatrix sampleSkin(const RuntimeClip& clip,float frame,uint32_t bone,uint32_t count,bool loop=true){
    float end=float(clip.frames-1);
    if(loop&&end>0){frame=std::fmod(frame,end);if(frame<0)frame+=end;}
    else frame=std::clamp(frame,0.f,end);
    uint32_t first=uint32_t(frame),second=std::min(first+1,clip.frames-1);
    return blendSkin(clip.matrices[size_t(first)*count+bone],clip.matrices[size_t(second)*count+bone],frame-first);
}
