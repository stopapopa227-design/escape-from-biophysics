#pragma once
struct Locker { Tile tile,wall; float door=0; };
std::vector<Locker> lockers;
int hiddenLocker=-1,knownLocker=-1;
float lockerSearch=0;
V3 lockerOut(const Locker& l){return {float(l.wall.x),0,float(l.wall.z)};}
V3 lockerPosition(const Locker& l){return center(l.tile)+lockerOut(l)*1.20f;}
V3 lockerApproach(const Locker& l){return center(l.tile)+lockerOut(l)*.30f;}
bool insideLocker(V3 p,const Locker& l,float margin=0){
    V3 q=p-lockerPosition(l),d=lockerOut(l);
    return std::abs(dot(q,d))<.34f+margin&&std::abs(q.x*d.z-q.z*d.x)<.46f+margin;
}
void generateLockers(){
    lockers.clear();hiddenLocker=knownLocker=-1;lockerSearch=0;
    for(Tile t:openTiles){
        if(t==exitTile||distance2D(center(t),teacher)<3)continue;
        bool report=false;for(const auto& n:notes)if(n.tile==t)report=true;
        if(report||(t.x*13+t.z*7)%5!=0)continue;
        bool crowded=false;for(const auto& l:lockers)if(distance2D(center(t),center(l.tile))<8)crowded=true;
        if(crowded)continue;
        for(Tile d:dirs)if(!walkable(t.x+d.x,t.z+d.z)){lockers.push_back({t,d});break;}
    }
}
