#pragma once
struct ProfessorAnimation { float phase=0,blend=0,facing=0,headTurn=0,time=0; } professorAnimation;
float smoothStep(float low,float high,float value){float t=std::clamp((value-low)/(high-low),0.0f,1.0f);return t*t*(3-2*t);}
V3 rotateX(V3 v,float angle){float c=std::cos(angle),s=std::sin(angle);return {v.x,c*v.y-s*v.z,s*v.y+c*v.z};}
V3 rotateY(V3 v,float angle){float c=std::cos(angle),s=std::sin(angle);return {c*v.x+s*v.z,v.y,-s*v.x+c*v.z};}
V3 rotateXC(V3 v,float c,float s){return {v.x,c*v.y-s*v.z,s*v.y+c*v.z};}
V3 rotateYC(V3 v,float c,float s){return {c*v.x+s*v.z,v.y,-s*v.x+c*v.z};}
float angleDelta(float a,float b){return std::atan2(std::sin(a-b),std::cos(a-b));}
void advanceProfessor(ProfessorAnimation& a,V3 before,V3 after,V3 targetPosition,float dt,bool alerting) {
    if(dt<=0)return;
    V3 motion=after-before;float distance=length(motion),speed=distance/dt;
    a.blend+=(std::clamp(speed/1.35f,0.0f,1.0f)-a.blend)*(1-std::exp(-dt*10));
    a.phase+=distance*(2*PI/1.04f);a.time+=dt;
    if(distance>0.0001f){float desired=std::atan2(-motion.x,-motion.z);a.facing+=std::clamp(angleDelta(desired,a.facing),-dt*4.5f,dt*4.5f);}
    float look=alerting?std::clamp(angleDelta(std::atan2(-(targetPosition.x-after.x),-(targetPosition.z-after.z)),a.facing),-0.40f,0.40f):0.04f*std::sin(a.time*0.8f);
    a.headTurn+=(look-a.headTurn)*(1-std::exp(-dt*6));
}
struct LegPose { V3 hip,knee,ankle; float upper,lower; };
LegPose legPose(const ProfessorAnimation& a,int side) {
    float sign=side==0?-1.0f:1.0f;
    float cycle=std::fmod(a.phase/(2*PI)+side*0.5f,1.0f);if(cycle<0)cycle+=1;
    float z,lift;
    if(cycle<0.6f){z=-0.312f+cycle/0.6f*0.624f;lift=0;}
    else {float swing=(cycle-0.6f)/0.4f;z=0.312f-0.624f*smoothStep(0,1,swing);lift=std::sin(swing*PI)*0.115f;}
    LegPose pose;
    pose.hip={sign*0.118f,0.79f-0.01f*a.blend*std::cos(a.phase*2),0};
    pose.ankle={sign*0.118f,0.11f+lift*a.blend,z*a.blend};
    V3 delta=pose.ankle-pose.hip;float d=length(delta);V3 axis=delta*(1/d);
    constexpr float thigh=0.40f,shin=0.35f;
    float along=(thigh*thigh-shin*shin+d*d)/(2*d);
    float h=std::sqrt(std::max(0.0f,thigh*thigh-along*along));
    pose.knee=pose.hip+axis*along+V3{0,-axis.z,axis.y}*h;
    V3 up=pose.knee-pose.hip,down=pose.ankle-pose.knee;
    pose.upper=std::atan2(-up.z,-up.y);pose.lower=std::atan2(-down.z,-down.y);return pose;
}
void animateProfessor(Mesh& output,V3 position,const ProfessorAnimation& a) {
    float sway=0.016f*a.blend*std::sin(a.phase),drop=-0.07f-0.01f*a.blend*std::cos(a.phase*2);
    float twist=0.035f*a.blend*std::sin(a.phase),breathe=0.004f*std::sin(a.time*1.8f);
    float blinkCycle=std::fmod(a.time,4.1f),blink=0;
    if(blinkCycle>3.80f&&blinkCycle<4.02f)blink=std::sin((blinkCycle-3.80f)/0.22f*PI);
    float cf=std::cos(a.facing),sf=std::sin(a.facing),ct=std::cos(twist),st=std::sin(twist),ch=std::cos(a.headTurn),sh=std::sin(a.headTurn);
    for(size_t part=0;part<professorParts.size();++part) {
        LegPose leg{};if(part==1||part==2)leg=legPose(a,int(part)-1);
        float cu=std::cos(leg.upper),su=std::sin(leg.upper),cl=std::cos(leg.lower),sl=std::sin(leg.lower);
        float phase=a.phase+(part==3?PI:0),shoulder=std::sin(phase)*0.24f*a.blend;
        float elbow=(part==4?0.20f:0.12f)+0.18f*a.blend*(0.5f+0.5f*std::sin(phase));
        float cs=std::cos(shoulder),ss=std::sin(shoulder),ce=std::cos(elbow),se=std::sin(elbow);
        for(Vertex v:professorParts[part].mesh){
            V3 p=v.p,n=v.n;
            if(part==1||part==2){
                float side=part==1?-1.0f:1.0f;V3 hip{side*0.118f,0.86f,0},knee{side*0.118f,0.46f,0};
                V3 upper=leg.hip+rotateXC(p-hip,cu,su),lower=leg.knee+rotateXC(p-knee,cl,sl);
                float kneeWeight=1-smoothStep(0.425f,0.495f,p.y);
                V3 q=upper*(1-kneeWeight)+lower*kneeWeight;
                V3 normal=rotateXC(n,cu,su)*(1-kneeWeight)+rotateXC(n,cl,sl)*kneeWeight;
                float shoeWeight=1-smoothStep(0.16f,0.24f,p.y);
                V3 foot=p+(leg.ankle-V3{side*0.118f,0.11f,0});
                p=q*(1-shoeWeight)+foot*shoeWeight;n=normalized(normal*(1-shoeWeight)+n*shoeWeight);
                p.x+=sway*0.3f;
            } else if(part>=3){
                V3 elbowPivot{part==3?-0.31f:0.31f,1.06f,-0.025f};
                float weight=1-smoothStep(1.01f,1.12f,p.y);
                p=p*(1-weight)+(elbowPivot+rotateXC(p-elbowPivot,ce,se))*weight;
                n=normalized(n*(1-weight)+rotateXC(n,ce,se)*weight);
                p=professorParts[part].pivot+rotateXC(p-professorParts[part].pivot,cs,ss);n=rotateXC(n,cs,ss);
                p=rotateYC(p,ct,st)+V3{sway,drop,0};n=rotateYC(n,ct,st);
            } else {
                // Blinking moves the separate ocular surfaces; frames remain rigid.
                if(v.material==13||v.material==14){p.y=1.742f+(p.y-1.742f)*(1-blink*0.97f);if(blink>0.94f)v.c={0.61f,0.38f,0.25f};}
                float headWeight=smoothStep(1.53f,1.59f,p.y);
                V3 neck{0,1.53f,0};V3 turned=neck+rotateYC(p-neck,ch,sh);
                p=p*(1-headWeight)+turned*headWeight;n=normalized(n*(1-headWeight)+rotateYC(n,ch,sh)*headWeight);
                if(p.y<1.5f&&p.y>1.1f)p.z+=breathe*std::max(0.0f,-n.z);
                p=rotateYC(p,ct,st)+V3{sway,drop,0};n=rotateYC(n,ct,st);
                if(p.y<0.87f&&v.material==Cloth)p.z+=0.008f*a.blend*std::sin(a.phase*2+p.x*9);
            }
            v.p=position+rotateYC(p,cf,sf);v.n=normalized(rotateYC(n,cf,sf));output.push_back(v);
        }
    }
}
