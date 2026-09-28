#pragma once

// Procedural geometry building blocks: primitive -> surface -> transform -> join.
// All texture coordinates stay in object space when animated.
enum Surface { Plain, Paint, Plaster, Stone, Ceiling, Cloth, Skin, Metal, Hair, Leather, Wood, Marble, Emblem, Eye, Eyelid, Paper };
void surface(Mesh& m,size_t start,int material) {
    for(size_t i=start;i<m.size();++i) m[i].material=float(material);
}
void materialBox(Mesh& m,V3 p,V3 half,V3 color,int material) {
    size_t start=m.size();box(m,p,half,color);surface(m,start,material);
}
void ellipsoid(Mesh& m,V3 p,V3 r,V3 color,int material,int slices=28,int rings=18) {
    auto vertex=[&](float u,float v) {
        V3 q{std::sin(v)*std::cos(u),std::cos(v),std::sin(v)*std::sin(u)};
        V3 point=p+V3{q.x*r.x,q.y*r.y,q.z*r.z};
        return Vertex{point,normalized({q.x/r.x,q.y/r.y,q.z/r.z}),color,point,float(material)};
    };
    for(int j=0;j<rings;++j) for(int i=0;i<slices;++i) {
        float u=float(i)*2*PI/slices,U=float(i+1)*2*PI/slices,v=float(j)*PI/rings,V=float(j+1)*PI/rings;
        auto a=vertex(u,v),b=vertex(U,v),c=vertex(U,V),d=vertex(u,V);
        for(auto q:{a,b,c,a,c,d})m.push_back(q);
    }
}
void tube(Mesh& m,V3 a,V3 b,float r0,float r1,V3 color,int material,int segments=16) {
    V3 axis=normalized(b-a),u=normalized(cross(axis,std::abs(axis.y)<0.9f?V3{0,1,0}:V3{1,0,0})),v=cross(axis,u);
    float slope=(r0-r1)/std::max(0.001f,length(b-a));
    for(int i=0;i<segments;++i) {
        float t=2*PI*i/segments,T=2*PI*(i+1)/segments;
        V3 radial=u*std::cos(t)+v*std::sin(t),radial2=u*std::cos(T)+v*std::sin(T);
        V3 n=normalized(radial+axis*slope),n2=normalized(radial2+axis*slope);
        V3 p=a+radial*r0,q=a+radial2*r0,r=b+radial2*r1,s=b+radial*r1;
        for(auto vert:{Vertex{p,n,color,p,float(material)},Vertex{q,n2,color,q,float(material)},Vertex{r,n2,color,r,float(material)},
                      Vertex{p,n,color,p,float(material)},Vertex{r,n2,color,r,float(material)},Vertex{s,n,color,s,float(material)}})m.push_back(vert);
    }
}
void ellipseRim(Mesh& m,V3 p,float rx,float ry,float thickness,V3 c,int mat,int segments=40) {
    for(int i=0;i<segments;++i) {
        float a=float(i)*2*PI/segments,b=float(i+1)*2*PI/segments;
        tube(m,p+V3{rx*std::cos(a),ry*std::sin(a),0},p+V3{rx*std::cos(b),ry*std::sin(b),0},thickness,thickness,c,mat,6);
    }
}
struct BodyPart { Mesh mesh; V3 pivot; float swing=0,phase=0; };
std::vector<BodyPart> professorParts;
void buildProfessor() {
    professorParts.clear();professorParts.resize(5);
    const V3 coat{0.36f,0.38f,0.40f},seam{0.22f,0.235f,0.25f},skin{0.61f,0.38f,0.25f},lips{0.40f,0.19f,0.15f};
    const V3 steel{0.23f,0.21f,0.16f},hair{0.25f,0.24f,0.21f},pants{0.10f,0.105f,0.11f};
    auto& body=professorParts[0].mesh;
    // Tailored coat: elliptical rings with cloth folds, rather than intersecting cubes.
    const float heights[]={0.70f,0.75f,0.85f,1.02f,1.18f,1.34f,1.44f,1.49f};
    const float widths[]={0.255f,0.265f,0.25f,0.23f,0.25f,0.28f,0.26f,0.19f};
    const float depths[]={0.145f,0.15f,0.15f,0.16f,0.175f,0.16f,0.13f,0.105f};
    for(int j=0;j<7;++j) for(int k=0;k<64;++k) {
        auto vert=[&](int row,int seg) {
            float a=seg*2*PI/64,fold=0.0035f*std::sin(a*13+row*0.6f);
            V3 p{(widths[row]+fold)*std::cos(a),heights[row],(depths[row]+fold)*std::sin(a)};
            V3 n=normalized({std::cos(a)/widths[row],0.035f,std::sin(a)/depths[row]});
            return Vertex{p,n,coat,p,float(Cloth)};
        };
        auto a=vert(j,k),b=vert(j,k+1),c=vert(j+1,k+1),d=vert(j+1,k);
        for(auto v:{a,b,c,a,c,d})body.push_back(v);
    }
    for(int k=0;k<64;++k) {
        float a=k*2*PI/64,b=(k+1)*2*PI/64;size_t start=body.size();
        quad(body,{0.19f*std::cos(a),1.49f,0.105f*std::sin(a)},
             {0.19f*std::cos(b),1.49f,0.105f*std::sin(b)},
             {0.087f*std::cos(b),1.525f,0.078f*std::sin(b)},
             {0.087f*std::cos(a),1.525f,0.078f*std::sin(a)},{0,1,0},coat);surface(body,start,Cloth);
    }
    ellipsoid(body,{0,1.48f,0},{0.092f,0.12f,0.084f},skin,Skin);
    // Project tailoring onto the actual triangulated coat, including its folds.
    // Thin surface patches have no box-shaped side walls and cannot float off the torso.
    const size_t coatVertices=7*64*6;
    auto coatVertex=[&](float x,float y,float lift,V3 color,int mat=Cloth) {
        Vertex result{};float front=100;
        for(size_t i=0;i<coatVertices;i+=3){
            const auto& a=body[i];const auto& b=body[i+1];const auto& c=body[i+2];
            float det=(b.p.y-c.p.y)*(a.p.x-c.p.x)+(c.p.x-b.p.x)*(a.p.y-c.p.y);
            if(std::abs(det)<1e-9f)continue;
            float u=((b.p.y-c.p.y)*(x-c.p.x)+(c.p.x-b.p.x)*(y-c.p.y))/det;
            float v=((c.p.y-a.p.y)*(x-c.p.x)+(a.p.x-c.p.x)*(y-c.p.y))/det;
            float w=1-u-v;
            if(u< -0.0001f||v< -0.0001f||w< -0.0001f)continue;
            float z=a.p.z*u+b.p.z*v+c.p.z*w;
            if(z>=front)continue;
            front=z;V3 pos{x,y,z-lift};
            result={pos,normalized(a.n*u+b.n*v+c.n*w),color,pos,float(mat)};
        }
        return result;
    };
    auto patch=[&](V3 a,V3 b,V3 c,V3 d,float lift,V3 color,int mat=Cloth){
        const int steps=12;
        auto vertex=[&](int i,int j){float u=float(i)/steps,v=float(j)/steps;
            V3 p=(a*(1-u)+b*u)*(1-v)+(d*(1-u)+c*u)*v;
            return coatVertex(p.x,p.y,lift,color,mat);};
        for(int j=0;j<steps;++j)for(int i=0;i<steps;++i){
            auto A=vertex(i,j),B=vertex(i+1,j),C=vertex(i+1,j+1),D=vertex(i,j+1);
            for(auto vert:{A,B,C,A,C,D})body.push_back(vert);
        }
    };
    auto stitch=[&](V3 a,V3 b,float lift,V3 color,float radius=0.0008f){
        for(int i=0;i<16;++i){V3 A=a+(b-a)*(i/16.0f),B=a+(b-a)*((i+1)/16.0f);
            tube(body,coatVertex(A.x,A.y,lift,color).p,coatVertex(B.x,B.y,lift,color).p,radius,radius,color,Cloth,5);}
    };
    patch({-.065f,1.485f,0},{.065f,1.485f,0},{.033f,1.235f,0},{-.033f,1.235f,0},.003f,{.72f,.77f,.78f});
    for(float side:{-1.0f,1.0f}) {
        V3 a{side*.065f,1.485f,0},b{side*.17f,1.40f,0},c{side*.045f,1.225f,0},d{side*.033f,1.40f,0};
        patch(a,b,c,d,.005f,{.405f,.425f,.445f});stitch(b,c,.006f,seam);
    }
    const V3 tie{.26f,.09f,.07f};
    patch({-.016f,1.435f,0},{.016f,1.435f,0},{.010f,1.402f,0},{-.010f,1.402f,0},.007f,tie);
    patch({-.010f,1.401f,0},{.010f,1.401f,0},{.024f,1.24f,0},{-.024f,1.24f,0},.007f,tie);
    patch({-.024f,1.24f,0},{.024f,1.24f,0},{0,1.21f,0},{0,1.21f,0},.007f,tie);
    for(int i=0;i<5;++i)ellipsoid(body,coatVertex(.025f,1.18f-i*.09f,.003f,steel).p,{.007f,.007f,.002f},steel,Metal,12,8);
    for(float side:{-1.0f,1.0f}){
        float x=side*.15f;
        patch({x-.052f,.96f,0},{x+.052f,.96f,0},{x+.044f,.84f,0},{x-.044f,.84f,0},.003f,coat);
        stitch({x-.052f,.96f,0},{x+.052f,.96f,0},.004f,seam);
        stitch({x-.052f,.96f,0},{x-.044f,.84f,0},.004f,seam,.0005f);
        stitch({x-.044f,.84f,0},{x+.044f,.84f,0},.004f,seam,.0005f);
        stitch({x+.044f,.84f,0},{x+.052f,.96f,0},.004f,seam,.0005f);
    }
    patch({-.18f,1.30f,0},{-.105f,1.30f,0},{-.11f,1.23f,0},{-.175f,1.23f,0},.003f,coat);
    stitch({-.18f,1.30f,0},{-.105f,1.30f,0},.004f,seam);
    for(int i=0;i<2;++i){float x=-.16f+i*.016f;
        stitch({x,1.303f,0},{x,1.35f,0},.004f,i==0?V3{.1f,.18f,.3f}:steel,.002f);}
    patch({.108f,1.31f,0},{.18f,1.31f,0},{.18f,1.274f,0},{.108f,1.274f,0},.003f,{.82f,.79f,.66f},Plain);
    for(int i=0;i<3;++i)stitch({.116f,1.301f-i*.009f,0},{.17f,1.301f-i*.009f,0},.004f,steel,.0005f);
    // Continuous anatomical head surface: cheeks and jaw share the same mesh.
    auto facePoint=[](float u,float v) {
        float y=1.738f+std::cos(v)*0.178f;
        float jaw=1.0f-0.17f*std::exp(-std::pow((y-1.61f)/0.055f,2.0f));
        float x=std::sin(v)*std::cos(u)*0.125f*jaw;
        float z=std::sin(v)*std::sin(u)*0.113f;
        float front=std::pow(std::max(0.0f,-std::sin(u)),6.0f);
        auto bump=[](float x0,float y0,float rx,float ry,float X,float Y){return std::exp(-std::pow((X-x0)/rx,2.0f)-std::pow((Y-y0)/ry,2.0f));};
        z-=front*(0.019f*bump(0,1.61f,0.09f,0.06f,x,y)+0.012f*bump(0,1.65f,0.09f,0.06f,x,y));
        z+=front*0.008f*(bump(-0.047f,1.743f,0.035f,0.023f,x,y)+bump(0.047f,1.743f,0.035f,0.023f,x,y));
        z-=front*(0.024f*bump(0,1.718f,0.014f,0.040f,x,y)+0.034f*bump(0,1.685f,0.021f,0.014f,x,y));
        z-=front*0.007f*(bump(-0.071f,1.694f,0.025f,0.025f,x,y)+bump(0.069f,1.696f,0.026f,0.025f,x,y));
        z-=front*0.006f*(bump(-0.044f,1.774f,0.033f,0.014f,x,y)+bump(0.044f,1.774f,0.033f,0.014f,x,y));
        return V3{x,y,z};
    };
    auto faceVertex=[&](float u,float v) {
        V3 p=facePoint(u,v),du=facePoint(u+0.001f,v)-facePoint(u-0.001f,v),dv=facePoint(u,v+0.001f)-facePoint(u,v-0.001f);
        V3 n=normalized(cross(du,dv));if(length(n)<0.5f)n=normalized({p.x,p.y-1.738f,p.z});
        return Vertex{p,n,skin,p,float(Skin)};
    };
    for(int j=0;j<80;++j)for(int k=0;k<112;++k) {
        float u=k*2*PI/112,U=(k+1)*2*PI/112,v=j*PI/80,V=(j+1)*PI/80;
        auto a=faceVertex(u,v),b=faceVertex(U,v),c=faceVertex(U,V),d=faceVertex(u,V);
        for(auto vertex:{a,b,c,a,c,d})body.push_back(vertex);
    }
    for(float side:{-1.0f,1.0f}) {
        ellipsoid(body,{side*0.128f,1.708f,0},{0.025f,0.046f,0.026f},skin,Skin);
        ellipsoid(body,{side*0.14f,1.71f,-0.018f},{0.011f,0.028f,0.009f},{0.39f,0.21f,0.14f},Skin,20,14);
        ellipseRim(body,{side*0.047f,1.742f,-0.107f},0.025f,0.012f,0.004f,skin,Eyelid,32);
        ellipsoid(body,{side*0.047f,1.742f,-0.109f},{0.023f,0.010f,0.007f},{0.70f,0.69f,0.61f},Eye,36,24);
        ellipsoid(body,{side*0.045f,1.741f,-0.116f},{0.007f,0.008f,0.003f},{0.18f,0.24f,0.20f},Eye,32,20);
        ellipsoid(body,{side*0.045f,1.741f,-0.119f},{0.003f,0.004f,0.001f},{0.015f,0.019f,0.015f},Eye,24,16);
        ellipsoid(body,{side*0.045f-0.002f,1.744f,-0.12f},{0.001f,0.001f,0.001f},{0.95f,0.96f,0.9f},Eye,12,8);
        tube(body,{side*0.018f,1.778f,-0.113f},{side*0.08f,1.77f,-0.1f},0.008f,0.005f,hair,Hair,12);
        ellipseRim(body,{side*0.05f,1.741f,-0.137f},0.041f,0.027f,0.0023f,steel,Metal);
        tube(body,{side*0.094f,1.745f,-0.131f},{side*0.127f,1.744f,0.024f},0.0025f,0.0025f,steel,Metal,8);
        ellipsoid(body,{side*0.016f,1.68f,-0.137f},{0.014f,0.011f,0.017f},skin,Skin);
        ellipsoid(body,{side*0.016f,1.673f,-0.146f},{0.006f,0.003f,0.005f},{0.25f,0.12f,0.08f},Skin,12,8);
        for(int i=0;i<3;++i) tube(body,{side*0.075f,1.719f-i*0.007f,-0.106f},{side*0.095f,1.715f-i*0.005f,-0.094f},0.0008f,0.0006f,lips,Skin,5);
        tube(body,{side*0.025f,1.673f,-0.119f},{side*0.042f,1.633f,-0.113f},0.0011f,0.0007f,lips,Skin,6);
    }
    tube(body,{-0.008f,1.747f,-0.142f},{0.008f,1.747f,-0.142f},0.0025f,0.0025f,steel,Metal,8);
    ellipsoid(body,{0,1.710f,-0.126f},{0.012f,0.031f,0.017f},skin,Skin,36,24);
    ellipsoid(body,{0,1.684f,-0.148f},{0.016f,0.014f,0.015f},skin,Skin,36,24);
    ellipsoid(body,{0,1.643f,-0.119f},{0.034f,0.006f,0.008f},lips,Skin);
    ellipsoid(body,{0,1.633f,-0.121f},{0.031f,0.006f,0.008f},{0.47f,0.25f,0.19f},Skin);
    tube(body,{-0.027f,1.638f,-0.127f},{0.027f,1.638f,-0.127f},0.0012f,0.0012f,{0.22f,0.10f,0.065f},Skin,8);
    for(int i=0;i<3;++i)for(int j=0;j<18;++j) {
        float y=1.795f+i*0.019f,v=std::acos((y-1.738f)/0.178f),u=-PI/2-0.55f+j*1.1f/18;
        V3 a=facePoint(u,v),b=facePoint(u+1.1f/18,v);a.z-=0.0005f;b.z-=0.0005f;
        tube(body,a,b,0.00045f,0.00045f,{0.48f,0.29f,0.19f},Skin,5);
    }
    // Individual grey hair wisps follow the scalp; receding hairline stays visible.
    for(int i=0;i<1600;++i) {
        float a=i*2.399963f,v=0.10f+float(i%137)/137*1.47f;
        V3 q{std::sin(v)*std::cos(a),std::cos(v),std::sin(v)*std::sin(a)};
        if(q.z< -0.42f&&v>0.55f)continue;
        float shade=0.20f+float(i%9)*0.035f;
        for(int segment=0;segment<3;++segment){
            float t=segment/3.0f,T=(segment+1)/3.0f;
            V3 p=facePoint(a+t*0.10f,v+t*0.08f),end=facePoint(a+T*0.10f,v+T*0.08f);
            p=p+normalized(p-V3{0,1.738f,0})*(0.0015f+std::sin(t*PI)*0.002f);
            end=end+normalized(end-V3{0,1.738f,0})*(0.0015f+std::sin(T*PI)*0.002f);
            tube(body,p,end,0.0008f*(1-t*0.65f),0.0008f*(1-T*0.65f),{shade,shade*0.98f,shade*0.91f},Hair,4);
        }
    }
    for(int side=0;side<2;++side) {
        float sign=side==0?-1.0f:1.0f;
        BodyPart& leg=professorParts[1+side];leg.pivot={sign*0.118f,0.86f,0};leg.swing=0.32f;leg.phase=side*PI;
        ellipsoid(leg.mesh,{sign*0.118f,0.59f,0},{0.092f,0.275f,0.097f},pants,Cloth);
        ellipsoid(leg.mesh,{sign*0.118f,0.29f,0},{0.071f,0.245f,0.075f},pants,Cloth);
        tube(leg.mesh,{sign*0.118f,0.12f,-0.068f},{sign*0.118f,0.61f,-0.09f},0.0015f,0.0015f,{0.16f,0.16f,0.16f},Cloth,6);
        ellipsoid(leg.mesh,{sign*0.118f,0.085f,-0.054f},{0.087f,0.070f,0.153f},{0.075f,0.044f,0.026f},Leather);
        ellipsoid(leg.mesh,{sign*0.118f,0.032f,-0.054f},{0.088f,0.023f,0.156f},{0.037f,0.029f,0.022f},Leather);
        for(int i=0;i<5;++i) tube(leg.mesh,{sign*0.118f-0.025f,0.142f-i*0.002f,-0.04f-i*0.015f},{sign*0.118f+0.025f,0.142f-i*0.002f,-0.048f-i*0.015f},0.002f,0.002f,{0.10f,0.08f,0.06f},Cloth,6);
        BodyPart& arm=professorParts[3+side];arm.pivot={sign*0.275f,1.405f,0};arm.swing=0.20f;arm.phase=(1-side)*PI;
        ellipsoid(arm.mesh,{sign*0.29f,1.24f,0},{0.094f,0.205f,0.097f},coat,Cloth);
        ellipsoid(arm.mesh,{sign*0.315f,0.97f,-0.045f},{0.070f,0.175f,0.080f},coat,Cloth);
        tube(arm.mesh,{sign*0.315f,0.82f,-0.045f},{sign*0.315f,0.86f,-0.045f},0.067f,0.068f,{0.86f,0.84f,0.76f},Cloth,24);
        ellipsoid(arm.mesh,{sign*0.315f,0.77f,-0.045f},{0.047f,0.072f,0.028f},skin,Skin);
        for(int knuckle=0;knuckle<4;++knuckle)ellipsoid(arm.mesh,{sign*0.315f-0.03f+knuckle*0.020f,0.744f,-0.070f},{0.009f,0.012f,0.006f},skin,Skin,16,12);
        if(side==0){
            // A thin band wraps the wrist; the round case rests on the band.
            for(int i=0;i<48;++i){
                float a=i*2*PI/48,b=(i+1)*2*PI/48;
                auto v=[&](float angle,float y){V3 p{-.315f+.042f*std::cos(angle),y,-.045f+.031f*std::sin(angle)};
                    return Vertex{p,normalized({std::cos(angle)/.042f,0,std::sin(angle)/.031f}),{.11f,.07f,.035f},p,float(Leather)};};
                auto A=v(a,.798f),B=v(b,.798f),C=v(b,.816f),D=v(a,.816f);
                for(auto vert:{A,B,C,A,C,D})arm.mesh.push_back(vert);
            }
            ellipseRim(arm.mesh,{-.315f,.807f,-.079f},.020f,.020f,.0015f,steel,Metal,32);
            ellipsoid(arm.mesh,{-.315f,.807f,-.078f},{.019f,.019f,.002f},{.72f,.70f,.58f},Plain,24,16);
            tube(arm.mesh,{-.315f,.807f,-.081f},{-.307f,.818f,-.081f},.0007f,.0007f,steel,Metal,6);
            tube(arm.mesh,{-.315f,.807f,-.081f},{-.325f,.803f,-.081f},.0007f,.0007f,steel,Metal,6);
        }
        for(int finger=0;finger<4;++finger) {
            float x=sign*0.315f-0.031f+finger*0.021f,y=0.71f+(finger==0||finger==3?0.012f:0);
            tube(arm.mesh,{x,0.75f,-0.054f},{x,y,-0.07f},0.009f,0.007f,skin,Skin,12);
            ellipsoid(arm.mesh,{x,y,-0.07f},{0.007f,0.012f,0.009f},skin,Skin,12,8);
            ellipsoid(arm.mesh,{x,y,-0.079f},{0.004f,0.006f,0.001f},{0.69f,0.48f,0.35f},Skin,10,6);
        }
        ellipsoid(arm.mesh,{sign*0.357f,0.773f,-0.062f},{0.016f,0.037f,0.017f},skin,Skin);
        if(side==1) {
            materialBox(arm.mesh,{0.318f,0.65f,-0.012f},{0.098f,0.142f,0.022f},{0.24f,0.055f,0.025f},Leather);
            materialBox(arm.mesh,{0.318f,0.65f,-0.037f},{0.090f,0.132f,0.006f},{0.66f,0.62f,0.48f},Plain);
            materialBox(arm.mesh,{0.318f,0.65f,-0.045f},{0.10f,0.145f,0.004f},{0.29f,0.07f,0.038f},Leather);
            for(int i=0;i<4;++i) materialBox(arm.mesh,{0.318f,0.70f-i*0.015f,-0.051f},{0.06f,0.002f,0.001f},{0.74f,0.56f,0.25f},Metal);
        }
    }
}

#include "materials.h"
