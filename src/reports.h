#pragma once
// Printed sheets on a thin clipboard, with real page edges and a curled corner.
// Typography is mipmapped so fine table lines remain stable at a distance.
Mesh buildLaboratoryReport(int index){
    Mesh mesh;
    materialBox(mesh,{0,1.55f,-1.504f},{.204f,.281f,.009f},{.20f,.155f,.105f},Wood);
    materialBox(mesh,{.002f,1.546f,-1.492f},{.187f,.261f,.0015f},{.64f,.635f,.59f},Paper);
    materialBox(mesh,{-.003f,1.552f,-1.488f},{.188f,.261f,.001f},{.77f,.765f,.715f},Paper);
    auto paperZ=[](float x,float y){return -1.484f+.007f*smoothStep(.08f,.19f,x)*smoothStep(1.42f,1.29f,y);};
    for(int j=0;j<20;++j)for(int i=0;i<12;++i){
        float x=-.185f+i*(.37f/12),X=x+.37f/12,y=1.289f+j*(.522f/20),Y=y+.522f/20;
        size_t start=mesh.size();
        V3 a{x,y,paperZ(x,y)},b{X,y,paperZ(X,y)},c{X,Y,paperZ(X,Y)},d{x,Y,paperZ(x,Y)};
        quad(mesh,a,b,c,d,normalized(cross(b-a,d-a)),{1,1,1});surface(mesh,start,Report);
        for(size_t k=start;k<mesh.size();++k)
            mesh[k].tex={(mesh[k].p.x+.185f)/.37f,(1.811f-mesh[k].p.y)/.522f,float(index%5)};
    }
    materialBox(mesh,{0,1.818f,-1.477f},{.042f,.016f,.004f},{.25f,.26f,.265f},Metal);
    tube(mesh,{-.029f,1.814f,-1.469f},{.029f,1.814f,-1.469f},.002f,.002f,{.39f,.40f,.40f},Metal,8);
    return mesh;
}
const Mesh& laboratoryReport(size_t index){
    static const std::array<Mesh,5> reports={buildLaboratoryReport(0),buildLaboratoryReport(1),buildLaboratoryReport(2),buildLaboratoryReport(3),buildLaboratoryReport(4)};
    return reports[index%reports.size()];
}