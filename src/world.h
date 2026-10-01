#pragma once
struct WallRun { Tile start,along,out; int count; };
std::vector<WallRun> wallRuns;
std::vector<V3> ceilingLights;
struct WallDecoration { Tile tile,direction; int kind; };
std::vector<WallDecoration> wallDecorations;
bool wallBoundary(Tile t,Tile d){return walkable(t.x,t.z)&&!walkable(t.x+d.x,t.z+d.z);}
void findWallRuns() {
    wallRuns.clear();
    for(Tile d:dirs)for(int z=1;z<N-1;++z)for(int x=1;x<N-1;++x){
        Tile t{x,z},along=d.x?Tile{0,1}:Tile{1,0};
        if(!wallBoundary(t,d)||wallBoundary({x-along.x,z-along.z},d))continue;
        int count=1;while(wallBoundary({x+along.x*count,z+along.z*count},d))++count;
        wallRuns.push_back({t,along,d,count});
    }
}
void placeKit(Mesh& result,const Mesh& kit,Tile location,Tile d) {
    V3 p=center(location);float angle=d.z==-1?0:d.z==1?PI:d.x==1?-PI/2:PI/2;
    float c=std::cos(angle),s=std::sin(angle);
    for(Vertex v:kit){V3 q=v.p,n=v.n;v.p=p+V3{c*q.x+s*q.z,q.y,-s*q.x+c*q.z};v.n={c*n.x+s*n.z,n.y,-s*n.x+c*n.z};if(v.material!=Emblem&&v.material!=Report&&v.material!=Poster)v.tex=v.p;result.push_back(v);}
}
// Offset the contour at each corner with a miter. Adjacent runs meet exactly;
// unlike expanded cubes they never create coplanar overlapping moldings.
V3 wallEndpoint(const WallRun& run,bool end,float y,float projection) {
    Tile t=run.start;if(end){t.x+=run.along.x*(run.count-1);t.z+=run.along.z*(run.count-1);}
    float sign=end?1.0f:-1.0f;
    bool openBeyond=walkable(t.x+int(sign)*run.along.x,t.z+int(sign)*run.along.z);
    float extension=(openBeyond?projection:-projection);
    V3 along{float(run.along.x),0,float(run.along.z)},out{float(run.out.x),0,float(run.out.z)};
    V3 point=center(t)+out*(CELL/2-projection)+along*(sign*(CELL/2+extension));point.y=y;return point;
}
void wallRibbon(const WallRun& run,float y0,float p0,float y1,float p1,V3 color,int material) {
    V3 a=wallEndpoint(run,false,y0,p0),b=wallEndpoint(run,true,y0,p0),c=wallEndpoint(run,true,y1,p1),d=wallEndpoint(run,false,y1,p1);
    V3 normal=normalized(cross(b-a,d-a));V3 expected{-float(run.out.x),0,-float(run.out.z)};
    if(std::abs(y1-y0)<0.0001f)normal={0,y0<0.01f?1.0f:(p1>p0?-1.0f:1.0f),0};else if(dot(normal,expected)<0)normal=normal*-1;
    size_t start=staticMesh.size();quad(staticMesh,a,b,c,d,normal,color);surface(staticMesh,start,material);
}
void emblemQuad(Mesh& m,V3 p,float width,float height) {
    V3 n{0,0,1},c{1,1,1};
    Vertex a{p+V3{-width/2,-height/2,0},n,c,{0,1,0},12},b{p+V3{width/2,-height/2,0},n,c,{1,1,0},12};
    Vertex C{p+V3{width/2,height/2,0},n,c,{1,0,0},12},d{p+V3{-width/2,height/2,0},n,c,{0,0,0},12};
    for(Vertex v:{a,b,C,a,C,d})m.push_back(v);
}
// Raised joinery profiles with bevels and recessed fields, inspired by the reference.
// Local +Z faces the corridor. Keep every wall panel inside the collision margin.
const V3 oakField{.69f,.56f,.38f},oakRail{.74f,.60f,.40f},oakEdge{.53f,.39f,.24f};
void woodPanel(Mesh& kit,float x,float y,float hx,float hy,float backing){
    const float inset[]={0,.012f,.024f,.038f,.050f};
    const float depth[]={.004f,.021f,.027f,.018f,.006f};
    auto ring=[&](int r,int corner){
        float sx=(corner==0||corner==3)?-1.0f:1.0f,sy=corner<2?-1.0f:1.0f;
        return V3{x+sx*(hx-inset[r]),y+sy*(hy-inset[r]),backing+depth[r]};
    };
    for(int r=0;r<4;++r)for(int k=0;k<4;++k){
        V3 a=ring(r,k),b=ring(r,(k+1)%4),c=ring(r+1,(k+1)%4),d=ring(r+1,k);
        V3 n=normalized(cross(b-a,d-a));if(n.z<0)n=n*-1;
        size_t start=kit.size();quad(kit,a,b,c,d,n,r==2?oakEdge:oakRail);surface(kit,start,Wood);
    }
    size_t start=kit.size();quad(kit,ring(4,0),ring(4,1),ring(4,2),ring(4,3),{0,0,1},oakField);surface(kit,start,Wood);
}
Mesh wallPanelKit(bool door){
    Mesh kit;
    for(int i=0;i<6;++i){float x=-1.25f+i*.5f;
        if(door&&std::abs(x)<.90f)continue;
        woodPanel(kit,x,.53f,.218f,.29f,-1.586f);
        woodPanel(kit,x,1.825f,.218f,.915f,-1.586f);
    }
    // The center rail is split around door casings; no overlays across a door leaf.
    for(int i=0;i<6;++i){float x=-1.25f+i*.5f;if(door&&std::abs(x)<.90f)continue;
        materialBox(kit,{x,.855f,-1.577f},{.25f,.025f,.016f},oakRail,Wood);
    }
    return kit;
}
// Department names: https://phys.msu.ru/kafedry.php
const std::array<std::array<const wchar_t*,3>,12> posterDepartments={{
    {{L"ТЕОРЕТИЧЕСКОЙ",L"ФИЗИКИ",L""}},
    {{L"КВАНТОВОЙ",L"СТАТИСТИКИ",L"И ТЕОРИИ ПОЛЯ"}},
    {{L"ОПТИКИ, СПЕКТРОСКОПИИ",L"И ФИЗИКИ",L"НАНОСИСТЕМ"}},
    {{L"ФИЗИКИ",L"ТВЕРДОГО ТЕЛА",L""}},
    {{L"МАГНЕТИЗМА",L"",L""}},
    {{L"АКУСТИКИ",L"",L""}},
    {{L"КВАНТОВОЙ",L"ЭЛЕКТРОНИКИ",L""}},
    {{L"ФИЗИКИ",L"КОЛЕБАНИЙ",L""}},
    {{L"НАНОФОТОНИКИ",L"",L""}},
    {{L"МАТЕМАТИЧЕСКОГО",L"МОДЕЛИРОВАНИЯ",L"И ИНФОРМАТИКИ"}},
    {{L"ФИЗИКИ ПОЛИМЕРОВ",L"И КРИСТАЛЛОВ",L""}},
    {{L"ФИЗИКИ ЧАСТИЦ",L"И КОСМОЛОГИИ",L""}}
}};
Mesh posterKit(size_t variant) {
    Mesh kit;
    materialBox(kit,{0,1.94f,-1.565f},{0.59f,0.70f,0.028f},{0.26f,0.16f,0.07f},Wood);
    materialBox(kit,{0,1.94f,-1.528f},{0.545f,0.655f,0.007f},{1,1,1},Paper);
    size_t first=kit.size();
    quad(kit,{-.545f,1.285f,-1.519f},{.545f,1.285f,-1.519f},{.545f,2.595f,-1.519f},{-.545f,2.595f,-1.519f},{0,0,1},{1,1,1});
    for(size_t i=first;i<kit.size();++i){auto& v=kit[i];v.material=Poster;v.tex={(v.p.x+.545f)/1.09f,(2.595f-v.p.y)/1.31f,float(variant%12)};}
    return kit;
}

Mesh classroomKit(int number) {
    Mesh kit;
    materialBox(kit,{0,1.34f,-1.577f},{0.72f,1.34f,0.022f},oakField,Wood);
    // Stepped architraves and head molding, not pilasters.
    for(float side:{-.78f,.78f}){
        materialBox(kit,{side,1.38f,-1.565f},{.075f,1.38f,.034f},oakEdge,Wood);
        materialBox(kit,{side,1.38f,-1.528f},{.052f,1.38f,.020f},oakRail,Wood);
        materialBox(kit,{side,1.38f,-1.505f},{.022f,1.38f,.008f},oakField,Wood);
    }
    materialBox(kit,{0,2.73f,-1.565f},{.86f,.08f,.034f},oakEdge,Wood);
    materialBox(kit,{0,2.73f,-1.528f},{.835f,.052f,.020f},oakRail,Wood);
    materialBox(kit,{0,2.79f,-1.521f},{.885f,.017f,.027f},oakRail,Wood);
    for(float side:{-.36f,.36f}){
        woodPanel(kit,side,.64f,.292f,.44f,-1.550f);
        woodPanel(kit,side,1.855f,.292f,.62f,-1.550f);
    }
    materialBox(kit,{0,1.34f,-1.546f},{.014f,1.32f,.012f},oakEdge,Wood);
    for(float side:{-0.10f,0.10f}){
        materialBox(kit,{side,1.23f,-1.518f},{0.023f,0.115f,0.012f},{0.50f,0.39f,0.17f},Metal);
        tube(kit,{side,1.18f,-1.455f},{side,1.31f,-1.455f},0.010f,0.010f,{0.58f,0.43f,0.21f},Metal,12);
    }
    materialBox(kit,{0,2.94f,-1.56f},{0.29f,0.115f,0.033f},{0.11f,0.09f,0.055f},Wood);
    wallText(kit,{-0.12f,2.995f,-1.520f},std::to_string(number),0.017f,{0.84f,0.76f,0.53f});return kit;
}
Mesh lockerKit(float opening){
    Mesh kit,door;V3 steel{.28f,.34f,.33f},edge{.13f,.18f,.18f};
    materialBox(kit,{0,1.13f,-1.53f},{.46f,1.08f,.018f},edge,Metal);
    for(float x:{-.45f,.45f})materialBox(kit,{x,1.13f,-1.20f},{.018f,1.08f,.33f},steel,Metal);
    for(float y:{.07f,2.20f})materialBox(kit,{0,y,-1.20f},{.46f,.025f,.34f},steel,Metal);
    // Door ventilation leaves a narrow view from inside the closed locker.
    materialBox(door,{0,.79f,-.856f},{.432f,.69f,.018f},steel,Metal);
    materialBox(door,{0,1.99f,-.856f},{.432f,.18f,.018f},steel,Metal);
    for(float x:{-.365f,.365f})materialBox(door,{x,1.645f,-.856f},{.067f,.165f,.018f},steel,Metal);
    for(int i=0;i<11;++i)materialBox(door,{0,1.49f+i*.030f,-.85f},{.30f,.008f,.025f},edge,Metal);
    materialBox(door,{.31f,1.10f,-.815f},{.025f,.105f,.018f},edge,Metal);
    tube(door,{.31f,1.03f,-.78f},{.31f,1.17f,-.78f},.012f,.012f,{.50f,.51f,.49f},Metal,10);
    for(float y:{.34f,1.98f})tube(kit,{-.455f,y-.045f,-.85f},{-.455f,y+.045f,-.85f},.025f,.025f,edge,Metal,10);
    V3 hinge{-.45f,0,-.85f};float angle=-opening*1.85f,c=std::cos(angle),s=std::sin(angle);
    for(Vertex v:door){V3 p=v.p-hinge,n=v.n;v.p=hinge+V3{c*p.x+s*p.z,p.y,-s*p.x+c*p.z};v.n={c*n.x+s*n.z,n.y,-s*n.x+c*n.z};v.tex=v.p;kit.push_back(v);}
    return kit;
}
void buildWorld(bool upload=true) {
    staticMesh.clear();staticMesh.reserve(200000);ceilingLights.clear();wallDecorations.clear();findWallRuns();
    // One continuous surface removes all per-cell floor and ceiling seams.
    materialBox(staticMesh,{N*CELL/2,-0.055f,N*CELL/2},{N*CELL/2,0.05f,N*CELL/2},{0.53f,0.49f,0.40f},Stone);
    materialBox(staticMesh,{N*CELL/2,3.86f,N*CELL/2},{N*CELL/2,0.06f,N*CELL/2},{0.84f,0.81f,0.72f},Ceiling);
    size_t posterIndex=0;
    for(const WallRun& run:wallRuns){
        wallRibbon(run,0,0.045f,0.18f,0.045f,{.46f,.34f,.21f},Wood);
        wallRibbon(run,0.18f,0.045f,0.20f,0,oakRail,Wood);
        wallRibbon(run,0.20f,0,2.75f,0,oakField,Wood);
        wallRibbon(run,2.75f,0,2.75f,0.028f,oakRail,Wood);
        wallRibbon(run,2.75f,0.028f,2.81f,0.028f,oakRail,Wood);
        wallRibbon(run,2.81f,0.028f,2.81f,0,oakRail,Wood);
        wallRibbon(run,2.81f,0,3.52f,0,{0.81f,0.78f,0.68f},Plaster);
        wallRibbon(run,3.52f,0,3.52f,0.035f,{0.87f,0.83f,0.73f},Plaster);
        wallRibbon(run,3.52f,0.035f,3.60f,0.035f,{0.87f,0.83f,0.73f},Plaster);
        wallRibbon(run,3.60f,0.035f,3.60f,0.075f,{0.87f,0.83f,0.73f},Plaster);
        wallRibbon(run,3.60f,0.075f,3.70f,0.075f,{0.87f,0.83f,0.73f},Plaster);
        wallRibbon(run,3.70f,0.075f,3.70f,0.11f,{0.87f,0.83f,0.73f},Plaster);
        wallRibbon(run,3.70f,0.11f,3.80f,0.11f,{0.87f,0.83f,0.73f},Plaster);
        wallRibbon(run,0.001f,0.14f,0.001f,0.30f,{0.22f,0.16f,0.105f},Marble);
        for(int i=0;i<run.count;++i){
            Tile tileAt{run.start.x+run.along.x*i,run.start.z+run.along.z*i};
            bool reportHere=false;for(const Note& note:notes)if(note.tile==tileAt&&noteWall(note.tile)==run.out)reportHere=true;
            bool lockerHere=false;for(const auto& l:lockers)if(l.tile==tileAt&&l.wall==run.out)lockerHere=true;
            int key=tileAt.x*17+tileAt.z*31+run.out.x*3+run.out.z*7;
            int kind=(key%7+7)%7;
            bool exitDoor=tileAt==exitTile&&run.out==Tile{0,-1};
            bool door=exitDoor||(!reportHere&&!lockerHere&&!(tileAt==exitTile)&&(kind==2||kind==5));
            placeKit(staticMesh,wallPanelKit(door),tileAt,run.out);
            if(tileAt==exitTile||reportHere||lockerHere)continue;
            Mesh kit;
            if(kind==0||kind==4){kit=posterKit(posterIndex++);kind=1;}
            else if(door){kit=classroomKit(300+tileAt.x+tileAt.z*2);kind=2;}
            else continue;
            wallDecorations.push_back({tileAt,run.out,kind});placeKit(staticMesh,kit,tileAt,run.out);
        }
    }
    for(Tile t:openTiles){
        V3 p=center(t);
        if((t.x+t.z)%3!=0)continue;
        ceilingLights.push_back(p+V3{0,3.57f,0});
        bool horizontal=walkable(t.x-1,t.z)&&walkable(t.x+1,t.z);
        Mesh fixture;
        materialBox(fixture,{0,3.72f,0},{0.25f,0.045f,0.74f},{0.49f,0.47f,0.40f},Metal);
        for(float side:{-0.12f,0.12f})tube(fixture,{side,3.64f,-0.65f},{side,3.64f,0.65f},0.031f,0.031f,{1.22f,1.16f,1.04f},Plain,12);
        for(Vertex v:fixture){if(horizontal){std::swap(v.p.x,v.p.z);std::swap(v.n.x,v.n.z);}v.p=v.p+p;v.tex=v.p;staticMesh.push_back(v);}
    }
    for(int z:{3,15}) {
        Mesh sign;materialBox(sign,{0,3.08f,0},{1.27f,0.24f,0.025f},{0.19f,0.115f,0.045f},Wood);
        for(float x:{-1.12f,1.12f})tube(sign,{x,3.32f,0},{x,3.80f,0},0.010f,0.010f,{0.35f,0.32f,0.24f},Metal,8);
        russianSign(sign,{-1.13f,3.20f,0.028f},L"ФИЗИЧЕСКИЙ ФАКУЛЬТЕТ",0.0195f,{0.92f,0.82f,0.57f});
        russianSign(sign,{-0.54f,3.00f,0.028f},L"МГУ  /  ЭТАЖ 3",0.014f,{0.88f,0.80f,0.60f});
        for(Vertex v:sign){V3 p=v.p,n=v.n;v.p=center({5,z})+V3{-p.z,p.y,p.x};v.n={-n.z,n.y,n.x};v.tex=v.p;staticMesh.push_back(v);}
    }
    V3 e=center(exitTile);Mesh exitKit=classroomKit(300);placeKit(staticMesh,exitKit,exitTile,{0,-1});
    materialBox(staticMesh,e+V3{0,2.94f,-1.50f},{0.42f,0.13f,0.04f},{0.10f,1.05f,0.48f},Plain);
    wallText(staticMesh,e+V3{-0.26f,3.015f,-1.452f},"EXIT",0.023f,{0.02f,0.12f,0.06f});
    if(upload){glBindBuffer(GL_ARRAY_BUFFER,worldVbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(staticMesh.size()*sizeof(Vertex)),staticMesh.data(),0x88E4);glBindBuffer(GL_ARRAY_BUFFER,vbo);}
}
