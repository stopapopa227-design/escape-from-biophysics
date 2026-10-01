"""Blender 4.0: editable, weighted professor asset. Run with --factory-startup -b -P.

Creates a new scene; saves only assets/models/teacher.blend and derived assets.
Coordinates: metres, Z up, character faces -Y. No external textures or add-ons.
"""
import bpy
import bmesh
import math
import json
from pathlib import Path
from mathutils import Vector, Quaternion
from mathutils.bvhtree import BVHTree
from mathutils.geometry import tessellate_polygon

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets' / 'models'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.name = 'Professor | rigged character'
scene.unit_settings.system = 'METRIC'
scene.render.fps = 30


def material(name, color, rough=.65, metal=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = rough
    bsdf.inputs['Metallic'].default_value = metal
    return m


skin = material('Skin | warm neutral', (.52, .31, .205), .58)
coat = material('Wool | graphite grey', (.19, .205, .225), .82)
lapel = material('Wool | tailored lapels', (.22, .235, .25), .76)
seam = material('Tailoring | dark stitching', (.105, .12, .14), .85)
shirt = material('Shirt | ivory cotton', (.78, .80, .78), .83)
trousers = material('Trousers | charcoal', (.075, .085, .105), .85)
leather = material('Shoes and watch strap | dark brown leather', (.055, .027, .014), .32)
sole = material('Shoe sole', (.022, .021, .023), .8)
tie = material('Tie | burgundy', (.21, .027, .035), .55)
steel = material('Glasses and watch | brushed metal', (.30, .29, .27), .24, .82)
hair = material('Hair | silver grey', (.23, .235, .24), .85)
white = material('Eyes | warm white', (.72, .69, .63), .25)
iris = material('Eyes | hazel iris', (.115, .16, .12), .25)
pupil = material('Eyes | pupil', (.009, .012, .014), .18)
lips = material('Lips and subtle creases', (.32, .125, .09), .7)
paper = material('Badge | ivory', (.82, .83, .78), .9)

pieces = []


def active(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def uv(name, center, scale, mat, group=None, seg=32, rings=20):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, location=center)
    o = bpy.context.object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.data.materials.append(mat)
    for p in o.data.polygons:
        p.use_smooth = True
    if group:
        weight(o, group)
    return o


def weight(obj, bone):
    g = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
    g.add(list(range(len(obj.data.vertices))), 1, 'REPLACE')
    pieces.append(obj)


def link_ellipsoid(name, a, b, rx, ry, mat):
    a,b=Vector(a),Vector(b)
    axis=(b-a).normalized()
    u=Vector((0,1,0)).cross(axis).normalized();v=axis.cross(u).normalized()
    start=a-axis*.026;end=b+axis*.026
    verts=[];faces=[];segments=32;rings=12
    for j in range(rings):
        t=j/(rings-1)
        # Match adjoining cross-sections at elbows and knees, without a pinched waist.
        profiles={'Jacket upper sleeve':(1,.82),'Jacket lower sleeve':(1.08,.85),
                  'Trouser thigh':(1,.83),'Trouser calf':(1.11,.89)}
        r0,r1=profiles.get(name,(1,.92))
        scale=r0*(1-t)+r1*t+.015*math.sin(math.pi*t)
        for i in range(segments):
            angle=i*math.tau/segments
            verts.append(start.lerp(end,t)+scale*(u*rx*math.cos(angle)+v*ry*math.sin(angle)))
    for j in range(rings-1):
        for i in range(segments):
            faces.append((j*segments+i,j*segments+(i+1)%segments,(j+1)*segments+(i+1)%segments,(j+1)*segments+i))
    faces += [tuple(reversed(range(segments))),tuple((rings-1)*segments+i for i in range(segments))]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);mesh.materials.append(mat)
    return o


def join(objects, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o


def fuse(objects, name, voxel, smooth=4):
    o = join(objects, name)
    mod = o.modifiers.new('Continuous joined surface', 'REMESH')
    mod.mode = 'VOXEL'
    mod.voxel_size = voxel
    mod.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    mod = o.modifiers.new('Relax junctions', 'SMOOTH')
    mod.factor = .6
    mod.iterations = smooth
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for p in o.data.polygons:
        p.use_smooth = True
    return o


def curve(name, points, radius, mat, bone, cyclic=False):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 2
    data.bevel_depth = radius
    data.bevel_resolution = 2
    s = data.splines.new('POLY')
    s.points.add(len(points)-1)
    for p, co in zip(s.points, points):
        p.co = (*co, 1)
    s.use_cyclic_u = cyclic
    o = bpy.data.objects.new(name, data)
    scene.collection.objects.link(o)
    data.materials.append(mat)
    active(o)
    bpy.ops.object.convert(target='MESH')
    weight(o, bone)
    return o


def patch(name, points, mat, bone, thickness=.0015):
    # Subdivide and project every vertex onto the actual garment, not a flat plane.
    verts=[];faces=[]
    lift=.014 if mat==tie else (.010 if mat==lapel else (.014 if 'collar' in name else (.004 if mat==shirt else .013)))
    for tri in tessellate_polygon([[Vector(p) for p in points]]):
        a,b,c=[Vector(points[k]) for k in tri] if isinstance(tri[0], int) else tri
        def vertex(i,j):
            q=a+(b-a)*(i/12)+(c-a)*(j/12)
            hit=coat_bvh.ray_cast(Vector((q.x,-1,q.z)),Vector((0,1,0)),2)[0]
            if hit is not None:q.y=hit.y-lift
            verts.append(tuple(q));return len(verts)-1
        for i in range(12):
            for j in range(12-i):
                faces.append((vertex(i,j),vertex(i+1,j),vertex(i,j+1)))
                if i+j<11:faces.append((vertex(i+1,j),vertex(i+1,j+1),vertex(i,j+1)))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);mesh.materials.append(mat)
    for p in mesh.polygons:p.use_smooth=True
    assign_surface(o)
    return o


# Rest pose landmarks. Arms in a relaxed A pose for painting shoulder weights.
landmarks = {}
for side, s in [('L', 1), ('R', -1)]:
    landmarks[side] = dict(shoulder=(s*.235, 0, 1.445), elbow=(s*.40, -.015, 1.20),
                           wrist=(s*.51, -.03, .98), hand=(s*.548, -.035, .875),
                           hip=(s*.112, 0, .91), knee=(s*.112, -.022, .52),
                           ankle=(s*.112, 0, .13), toe=(s*.112, -.18, .075))

# Skin is a genuinely connected surface, not merely a collection of overlapping limbs.
raw = [uv('Skin torso', (0, .005, 1.19), (.211, .121, .33), skin),
       uv('Pelvis', (0, .015, .91), (.19, .115, .18), skin),
       uv('Neck', (0, .005, 1.515), (.073, .067, .13), skin)]
head = uv('Head with sculpted facial planes', (0, 0, 0), (1, 1, 1), skin, seg=80, rings=56)
for v in head.data.vertices:
    q = v.co.copy()
    z = 1.737+q.z*.164
    jaw = 1-.18*math.exp(-((z-1.61)/.05)**2)
    x = q.x*.117*jaw
    y = q.y*.104
    front = max(0, -q.y)**7
    def bump(cx, cz, rx, rz):
        return math.exp(-((x-cx)/rx)**2-((z-cz)/rz)**2)
    y -= front*(.027*bump(0, 1.73, .015, .041)+.035*bump(0, 1.698, .02, .014)
                +.011*bump(0, 1.64, .06, .03)+.009*bump(0, 1.61, .07, .025))
    y += front*.011*(bump(-.044, 1.756, .033, .018)+bump(.044, 1.756, .033, .018))
    v.co = (x, y, z)
raw.append(head)
for side, d in landmarks.items():
    raw += [link_ellipsoid('Upper arm skin', d['shoulder'], d['elbow'], .061, .066, skin),
            link_ellipsoid('Forearm skin', d['elbow'], d['wrist'], .048, .05, skin),
            link_ellipsoid('Palm', d['wrist'], d['hand'], .038, .023, skin),
            link_ellipsoid('Thigh skin', d['hip'], d['knee'], .077, .078, skin),
            link_ellipsoid('Calf skin', d['knee'], d['ankle'], .056, .06, skin)]
    s = 1 if side == 'L' else -1
    raw.append(uv('Ear', (s*.116, .005, 1.735), (.023, .018, .037), skin))
    # Four visible fingers and an opposed thumb, merged into the palm.
    for i in range(4):
        x = s*(.525+i*.015)
        raw.append(link_ellipsoid('Finger', (x, -.04, .895), (x+s*.015, -.049, .833+abs(i-1.5)*.008), .009, .009, skin))
    raw.append(link_ellipsoid('Thumb', (s*.52, -.04, .931), (s*.502, -.07, .889), .012, .013, skin))
body = fuse(raw, 'Teacher | continuous skin', .008, 3)
# Omit occluded skin under opaque clothing, as in a game character mesh.
# This prevents the hidden torso from protruding through animated shoulders.
bm=bmesh.new();bm.from_mesh(body.data)
hidden=[]
for face in bm.faces:
    p=face.calc_center_median()
    visible_head=p.z>1.49 and abs(p.x)<.15
    visible_hand=p.z<1.025 and abs(p.x)>.43
    if not (visible_head or visible_hand):hidden.append(face)
bmesh.ops.delete(bm,geom=hidden,context='FACES')
bm.to_mesh(body.data);bm.free()

# Jacket and sleeves form another continuous surface, with a tailored waist.
def torso_mesh():
    rows = [(.83,.205,.122),(.89,.222,.13),(1.04,.205,.135),(1.19,.215,.143),
            (1.34,.238,.139),(1.445,.228,.113),(1.49,.14,.081)]
    verts=[]
    for z, w, depth in rows:
        for i in range(64):
            a=i*math.tau/64
            fold=.0016*math.sin(a*9+z*15)
            verts.append(((w+fold)*math.cos(a),(depth+fold)*math.sin(a),z))
    faces=[]
    for j in range(len(rows)-1):
        for i in range(64):
            faces.append((j*64+i,j*64+(i+1)%64,(j+1)*64+(i+1)%64,(j+1)*64+i))
    faces += [tuple(reversed(range(64))),tuple((len(rows)-1)*64+i for i in range(64))]
    m=bpy.data.meshes.new('Tailored coat topology');m.from_pydata(verts,[],faces);m.update()
    o=bpy.data.objects.new('Coat shell',m);scene.collection.objects.link(o);m.materials.append(coat)
    return o

shells=[torso_mesh()]
for side,d in landmarks.items():
    cuff=Vector(d['wrist']).lerp(Vector(d['elbow']),.12)
    shells += [link_ellipsoid('Jacket upper sleeve',d['shoulder'],d['elbow'],.079,.082,coat),
               link_ellipsoid('Jacket lower sleeve',d['elbow'],cuff,.060,.061,coat)]
jacket=fuse(shells,'Teacher | continuous jacket and sleeves',.008,9)

parts=[uv('Trousers waist',(0,.005,.86),(.187,.119,.16),trousers)]
for side,d in landmarks.items():
    parts += [link_ellipsoid('Trouser thigh',d['hip'],d['knee'],.092,.097,trousers),
              link_ellipsoid('Trouser calf',d['knee'],d['ankle'],.069,.078,trousers)]
pants=fuse(parts,'Teacher | continuous trousers',.008,9)


def smoothstep(lo,hi,x):
    t=max(0,min(1,(x-lo)/(hi-lo)))
    return t*t*(3-2*t)


def assign_surface(obj):
    for v in obj.data.vertices:
        x,y,z=v.co
        side='L' if x>=0 else 'R'
        ax=abs(x)
        if z>1.53:
            t=smoothstep(1.52,1.61,z);weights={'neck':1-t,'head':t}
        elif ax>.255 and z>.78:
            d=landmarks[side]
            p=Vector((ax,y,z));el=Vector((.40,-.015,1.20));wr=Vector((.51,-.03,.98))
            t=smoothstep(-.085,.085,(p-el).dot((wr-el).normalized()))
            h=1-smoothstep(.93,1.02,z)
            weights={f'upper_arm.{side}':(1-t)*(1-h),f'forearm.{side}':t*(1-h),f'hand.{side}':h}
            shoulder=smoothstep(.25,.32,ax)
            weights={k:w*shoulder for k,w in weights.items()}
            weights['chest']=1-shoulder
        elif z<.91 and obj!=jacket:
            t=1-smoothstep(.42,.63,z)
            hip=smoothstep(.79,.95,z)
            weights={f'thigh.{side}':(1-t)*(1-hip),f'shin.{side}':t*(1-hip),'pelvis':hip}
        else:
            if z<1.09:
                t=smoothstep(.91,1.09,z);weights={'pelvis':1-t,'spine':t}
            elif z<1.35:
                t=smoothstep(1.14,1.35,z);weights={'spine':1-t,'chest':t}
            else:
                t=smoothstep(1.46,1.55,z);weights={'chest':1-t,'neck':t}
        weights={k:w for k,w in weights.items() if w>.00001}
        total=sum(weights.values())
        for name,w in weights.items():
            g=obj.vertex_groups.get(name) or obj.vertex_groups.new(name=name)
            g.add([v.index],w/total,'REPLACE')
    pieces.append(obj)


for o in (body,jacket,pants):
    assign_surface(o)

def oxford_shoe(side, sign):
    # Foot-shaped last: narrow heel, high instep and a lower rounded toe box.
    rows=[(.077,.045,.104),(.062,.064,.134),(.015,.067,.151),
          (-.035,.073,.150),(-.085,.079,.133),(-.14,.078,.106),
          (-.19,.063,.086),(-.215,.040,.077),(-.227,.018,.062),(-.234,.002,.047)]
    x0=sign*.112;verts=[];faces=[];count=25
    for y,width,top in rows:
        for i in range(count):
            a=i*math.pi/(count-1)
            verts.append((x0+width*math.cos(a),y,.044+(top-.044)*max(0,math.sin(a))**.62))
    for j in range(len(rows)-1):
        for i in range(count-1):faces.append((j*count+i,j*count+i+1,(j+1)*count+i+1,(j+1)*count+i))
        faces.append((j*count+count-1,j*count,(j+1)*count,(j+1)*count+count-1))
    faces += [tuple(reversed(range(count))),tuple((len(rows)-1)*count+i for i in range(count))]
    mesh=bpy.data.meshes.new('Oxford leather last');mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new('Oxford upper '+side,mesh);scene.collection.objects.link(o);mesh.materials.append(leather)
    for poly in mesh.polygons:poly.use_smooth=True
    active(o)
    smooth=o.modifiers.new('Smooth leather last','SUBSURF');smooth.levels=2
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    weight(o,'foot.'+side)
    outline=[(x0-w-.003,y) for y,w,_ in rows]+[(x0+w+.003,y) for y,w,_ in reversed(rows)]
    n=len(outline)
    verts=[(x,y,z) for z in (.026,.046) for x,y in outline]
    faces=[tuple(reversed(range(n))),tuple(n+i for i in range(n))]
    for i in range(n):faces.append((i,(i+1)%n,(i+1)%n+n,i+n))
    mesh=bpy.data.meshes.new('Outsole profile');mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new('Oxford outsole '+side,mesh);scene.collection.objects.link(o);mesh.materials.append(sole)
    active(o);bevel=o.modifiers.new('Rounded welt edges','BEVEL');bevel.width=.004;bevel.segments=3
    bpy.ops.object.modifier_apply(modifier=bevel.name);weight(o,'foot.'+side)
    bpy.ops.mesh.primitive_cube_add(size=1,location=(x0,.037,.022))
    o=bpy.context.object;o.name='Oxford heel '+side;o.dimensions=(.113,.077,.029)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(sole);bevel=o.modifiers.new('Heel bevel','BEVEL');bevel.width=.007;bevel.segments=3
    bpy.ops.object.modifier_apply(modifier=bevel.name);weight(o,'foot.'+side)
    curve('Stitched welt '+side,[(x,y,.047) for x,y in outline],.0012,seam,'foot.'+side,True)
    def surface_z(x,y):
        for (Y,W,Z),(y2,w2,z2) in zip(rows,rows[1:]):
            if y2<=y<=Y:
                t=(Y-y)/(Y-y2);w=W*(1-t)+w2*t;z=Z*(1-t)+z2*t
                return .044+(z-.044)*max(0,1-(x/w)**2)**.31
        return .11
    # Curved toe-cap stitching follows the upper rather than crossing the air.
    pts=[]
    for i in range(25):
        x=-.068+i*.136/24;y=-.135-.012*(1-(x/.068)**2)
        pts.append((x0+x,y,surface_z(x,y)+.0018))
    curve('Toe cap seam '+side,pts,.0011,seam,'foot.'+side)
    tongue=[]
    for x,y in [(-.027,-.025),(.027,-.025),(.027,-.13),(-.027,-.13)]:
        tongue.append((x0+x,y,surface_z(x,y)+.002))
    mesh=bpy.data.meshes.new('Oxford tongue');mesh.from_pydata(tongue,[],[(0,1,2,3)]);mesh.update()
    o=bpy.data.objects.new('Oxford tongue '+side,mesh);scene.collection.objects.link(o);mesh.materials.append(leather);weight(o,'foot.'+side)
    for i in range(5):
        y=-.043-i*.017
        for x in (-.025,.025):
            z=surface_z(x,y)+.003
            curve('Lace eyelet '+side,[(x0+x+.003*math.cos(j*math.tau/12),y+.003*math.sin(j*math.tau/12),z) for j in range(12)],.00065,steel,'foot.'+side,True)
        curve('Crossed lace '+side,[(x0-.025,y,surface_z(-.025,y)+.004),(x0,y-.004,surface_z(0,y-.004)+.004),(x0+.025,y-.008,surface_z(.025,y-.008)+.004)],.0014,seam,'foot.'+side)


# Garment detailing follows the chest silhouette, never a floating cuboid.
coat_bvh=BVHTree.FromPolygons([v.co for v in jacket.data.vertices],[p.vertices[:] for p in jacket.data.polygons])
patch('Shirt front',[(-.064,-.084,1.48),(.064,-.084,1.48),(.039,-.142,1.27),(-.039,-.142,1.27)],shirt,'chest')
for side,s in [('L',1),('R',-1)]:
    patch('Notched lapel '+side,[(s*.059,-.09,1.475),(s*.174,-.099,1.407),
          (s*.118,-.130,1.354),(s*.131,-.126,1.329),(s*.025,-.15,1.16),(s*.039,-.146,1.355)],lapel,'chest')
    patch('Shirt collar '+side,[(s*.012,-.078,1.505),(s*.069,-.081,1.476),
          (s*.045,-.118,1.411),(s*.006,-.121,1.457)],shirt,'chest')
    # Inset pocket welts instead of solid patch pockets.
    curve('Pocket welt '+side,[(s*.078,-.137,1.012),(s*.17,-.095,1.027)],.0021,seam,'spine')
    d=landmarks[side]
    uv('Shirt cuff '+side,Vector(d['wrist']).lerp(Vector(d['elbow']),.08),(.054,.048,.035),shirt,'forearm.'+side)
    oxford_shoe(side,s)

uv('Tie knot',(0,-.123,1.445),(.014,.009,.02),tie,'chest')
patch('Silk tie',[(-.010,-.129,1.43),(.010,-.129,1.43),(.021,-.15,1.242),(0,-.151,1.219),(-.021,-.15,1.242)],tie,'chest')
for z in (1.155,1.066,.974):
    uv('Horn jacket button',(.012,-.146 if z>1.1 else -.137,z),(.006,.003,.006),leather,'spine',16,12)
curve('Breast pocket welt',[(-.172,-.103,1.335),(-.102,-.134,1.335)],.002,seam,'chest')
patch('Small faculty badge',[(.10,-.133,1.342),(.159,-.108,1.342),(.159,-.11,1.312),(.10,-.135,1.312)],paper,'chest')

# Face, ears and spectacles all follow the head bone.
for side,s in [('L',1),('R',-1)]:
    uv('Ear concha '+side,(s*.128,-.008,1.735),(.009,.004,.020),lips,'head')
    uv('Eye '+side,(s*.045,-.097,1.755),(.025,.012,.012),white,'head',40,24)
    uv('Iris '+side,(s*.045,-.1088,1.755),(.0075,.0017,.0075),iris,'head')
    uv('Pupil '+side,(s*.045,-.1103,1.755),(.0034,.001,.0034),pupil,'head')
    for upper in (True,False):
        points=[]
        for i in range(21):
            a=math.pi*i/20
            points.append((s*.045+.025*math.cos(a),-.104-.003*math.sin(a),1.755+(.010 if upper else -.008)*math.sin(a)))
        curve('Eyelid '+side,points,.0028,skin,'head')
    curve('Grey brow '+side,[(s*(.018+i*.0025),-.098,1.785+.003*math.sin(i/24*math.pi)) for i in range(25)],.0034,hair,'head')
    curve('Glasses oval '+side,[(s*.047+.039*math.cos(i*math.tau/64),-.139,1.755+.024*math.sin(i*math.tau/64)) for i in range(64)],.0017,steel,'head',True)
    curve('Glasses temple '+side,[(s*.086,-.139,1.76),(s*.115,-.081,1.77),(s*.121,.019,1.77),(s*.117,.031,1.751)],.0018,steel,'head')
    uv('Nostril '+side,(s*.012,-.125,1.689),(.004,.002,.0024),lips,'head',16,12)
curve('Glasses bridge',[(-.009,-.141,1.758),(0,-.144,1.763),(.009,-.141,1.758)],.0017,steel,'head')
curve('Upper lip',[(-.028,-.111,1.661),(-.010,-.121,1.664),(0,-.123,1.661),(.010,-.121,1.664),(.028,-.111,1.661)],.0027,lips,'head')
curve('Lower lip',[(-.025,-.112,1.658),(0,-.122,1.654),(.025,-.112,1.658)],.003,skin,'head')

# Receding silver hair: small curved strands on the back and temples.
for i in range(360):
    a=i*2.399963
    v=.38+(i%97)/97*1.14
    if math.sin(a)<-.42:
        continue
    points=[]
    for j in range(5):
        u=a+j*.022;v2=v+j*.012
        points.append((.119*math.sin(v2)*math.cos(u),.108*math.sin(v2)*math.sin(u),1.737+.166*math.cos(v2)))
    curve('Silver strand',points,.00065,hair,'head')

# A round wristwatch, with a band that wraps around the arm.
center=Vector(landmarks['L']['wrist'])
curve('Watch leather band',[(center.x+.047*math.cos(i*math.tau/48),center.y+.038*math.sin(i*math.tau/48),center.z+.004) for i in range(48)],.007,leather,'hand.L',True)
uv('Watch case',(center.x,center.y-.041,center.z+.004),(.021,.005,.021),steel,'hand.L')
uv('Watch dial',(center.x,center.y-.047,center.z+.004),(.018,.001,.018),paper,'hand.L')
curve('Watch hour hand',[(center.x,center.y-.049,center.z+.004),(center.x+.008,center.y-.049,center.z+.011)],.0008,steel,'hand.L')
curve('Watch minute hand',[(center.x,center.y-.049,center.z+.004),(center.x-.005,center.y-.049,center.z+.017)],.0006,steel,'hand.L')

# All visible parts are one exportable mesh object, with disconnected islands only
# where real objects (eyes, glasses, clothes) are physically separate.
character=join(pieces,'Teacher_Surface')
arm=bpy.data.armatures.new('Teacher_Skeleton')
rig=bpy.data.objects.new('Teacher_Rig',arm)
scene.collection.objects.link(rig)
active(rig)
bpy.ops.object.mode_set(mode='EDIT')


def bone(name, head, tail, parent=None):
    b=arm.edit_bones.new(name);b.head=head;b.tail=tail
    if parent:b.parent=arm.edit_bones[parent]
    return b


bone('root',(0,0,0),(0,0,.20))
bone('pelvis',(0,0,.89),(0,0,1.04),'root')
bone('spine',(0,0,1.04),(0,0,1.27),'pelvis')
bone('chest',(0,0,1.27),(0,0,1.48),'spine')
bone('neck',(0,0,1.48),(0,0,1.59),'chest')
bone('head',(0,0,1.59),(0,0,1.90),'neck')
for side,d in landmarks.items():
    bone('clavicle.'+side,(0,0,1.445),d['shoulder'],'chest')
    bone('upper_arm.'+side,d['shoulder'],d['elbow'],'clavicle.'+side)
    bone('forearm.'+side,d['elbow'],d['wrist'],'upper_arm.'+side)
    bone('hand.'+side,d['wrist'],d['hand'],'forearm.'+side)
    bone('thigh.'+side,d['hip'],d['knee'],'pelvis')
    bone('shin.'+side,d['knee'],d['ankle'],'thigh.'+side)
    bone('foot.'+side,d['ankle'],d['toe'],'shin.'+side)
bpy.ops.object.mode_set(mode='OBJECT')
rig.show_in_front=True
arm.display_type='OCTAHEDRAL'
character.parent=rig
mod=character.modifiers.new('Skeleton | four weights per vertex','ARMATURE');mod.object=rig
mod.use_deform_preserve_volume=False  # same linear skinning as OpenGL/glTF

# Three editable bone animation clips. Root remains in place for game navigation.
rig.animation_data_create()


def rotation(name,axis,angle):
    pb=rig.pose.bones[name]
    local=pb.bone.matrix_local.to_quaternion().inverted()@Vector(axis)
    pb.rotation_mode='QUATERNION'
    pb.rotation_quaternion=Quaternion(local,angle)


def make_action(name,frames,kind):
    action=bpy.data.actions.new(name);action.use_fake_user=True
    rig.animation_data.action=action
    for frame in range(1,frames+1):
        phase=(frame-1)/(frames-1)*math.tau
        for p in rig.pose.bones:
            p.location=(0,0,0);p.rotation_mode='QUATERNION';p.rotation_quaternion=(1,0,0,0)
        if kind=='walk':
            rig.pose.bones['pelvis'].location.y=.008*(1-math.cos(phase*2))
            rotation('spine',(0,0,1),.035*math.sin(phase))
            rotation('chest',(0,0,1),-.055*math.sin(phase))
        elif kind=='idle':
            rotation('chest',(1,0,0),.007*math.sin(phase))
            rotation('head',(0,0,1),.035*math.sin(phase))
        else:
            amount=math.sin(min(1,(frame-1)/(frames-1)*1.4)*math.pi/2)
            rotation('chest',(1,0,0),.10*amount)
        for side,s in [('L',1),('R',-1)]:
            ph=phase+(0 if side=='L' else math.pi)
            stride=.34*math.sin(ph) if kind=='walk' else 0
            knee=.12+.48*max(0,-math.sin(ph)) if kind=='walk' else .045
            rotation('thigh.'+side,(1,0,0),stride)
            rotation('shin.'+side,(1,0,0),-knee)
            rotation('foot.'+side,(1,0,0),-stride+knee)
            rotation('upper_arm.'+side,(0,1,0),s*.40)
            base=rig.pose.bones['upper_arm.'+side].rotation_quaternion.copy()
            rotation('upper_arm.'+side,(1,0,0),-.27*math.sin(ph) if kind=='walk' else (.95*amount if kind=='reach' else 0))
            rig.pose.bones['upper_arm.'+side].rotation_quaternion=base@rig.pose.bones['upper_arm.'+side].rotation_quaternion
            rotation('forearm.'+side,(1,0,0),.16 if kind!='reach' else .25*amount)
        # Place the lowest sole on the floor for each sampled pose.
        bpy.context.view_layer.update()
        evaluated=character.evaluated_get(bpy.context.evaluated_depsgraph_get())
        lowest=min(v.co.z for v in evaluated.data.vertices)
        rig.pose.bones['root'].location=arm.bones['root'].matrix_local.to_quaternion().inverted()@Vector((0,0,.004-lowest))
        for p in rig.pose.bones:
            p.keyframe_insert('location',frame=frame,group=p.name)
            p.keyframe_insert('rotation_quaternion',frame=frame,group=p.name)
    for fc in action.fcurves:
        for k in fc.keyframe_points:k.interpolation='LINEAR'
    rig.animation_data.action=None
    track=rig.animation_data.nla_tracks.new();track.name=name
    strip=track.strips.new(name,1,action)
    track.mute=True
    return action


idle=make_action('Idle',61,'idle')
walk=make_action('Walk',33,'walk')
reach=make_action('Catch',25,'reach')
rig.animation_data.action=idle
scene.frame_start=1;scene.frame_end=61;scene.frame_set(1)

# Validation matches the intended four-weight GPU skinning path.
max_weights=0
for v in character.data.vertices:
    groups=[g for g in v.groups if g.weight>0]
    assert groups and abs(sum(g.weight for g in groups)-1)<1e-4, 'Unweighted vertex'
    assert len(groups)<=4, 'More than four influences'
    assert all(character.vertex_groups[g.group].name in arm.bones for g in groups)
    max_weights=max(max_weights,len(groups))
assert len(arm.bones)==20
validation={'vertices':len(character.data.vertices),'polygons':len(character.data.polygons),
            'bones':len(arm.bones),'max_weights':max_weights,'clips':['Idle','Walk','Catch'],
            'front':'-Y','up':'Z','units':'metres','blender':bpy.app.version_string}
(OUT/'teacher-validation.json').write_text(json.dumps(validation,indent=2),encoding='utf8')

# Export only the character and skeleton, not the preview studio.
active(character);rig.select_set(True)
rig.animation_data.action=None
for track in rig.animation_data.nla_tracks:track.mute=False
bpy.ops.export_scene.gltf(filepath=str(OUT/'teacher.glb'),export_format='GLB',use_selection=True,
    export_animations=True,export_nla_strips=True,export_skins=True,export_yup=True)
for track in rig.animation_data.nla_tracks:track.mute=True
rig.animation_data.action=idle

# Studio preview, kept in its own collection in the editable .blend.
studio=bpy.data.collections.new('Preview studio | not exported');scene.collection.children.link(studio)


def to_studio(obj):
    for c in list(obj.users_collection):c.objects.unlink(obj)
    studio.objects.link(obj)


bpy.ops.mesh.primitive_plane_add(size=200)
floor=bpy.context.object;floor.name='Studio floor';floor.data.materials.append(material('Studio slate',(.043,.057,.073),.9));to_studio(floor)
world=bpy.data.worlds.new('Studio world');scene.world=world;world.use_nodes=True
world.node_tree.nodes['Background'].inputs[0].default_value=(.11,.14,.19,1)
world.node_tree.nodes['Background'].inputs[1].default_value=.3


def point_at(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


for name,pos,power,size in [('Key',(-3,-4,5),450,4),('Fill',(3,-2,3),230,3),('Rim',(1,2,4),550,3)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new(name,data);studio.objects.link(o);o.location=pos;point_at(o,(0,0,1))
data=bpy.data.cameras.new('Teacher preview camera');camera=bpy.data.objects.new('Teacher preview camera',data)
studio.objects.link(camera);scene.camera=camera;camera.location=(2.8,-5,2.35);data.lens=70;point_at(camera,(0,0,.99))
scene.render.engine='BLENDER_EEVEE'
scene.eevee.use_gtao=True;scene.eevee.gtao_distance=.16;scene.eevee.gtao_factor=1.25
scene.eevee.taa_render_samples=96
scene.render.resolution_x=960;scene.render.resolution_y=1080;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=False
rig['README']='20 deform bones; one mesh; normalized <=4 influences. Idle / Walk / Catch actions. Rest pose: relaxed A. Front -Y, metres. GLB exported separately.'
character['topology']='Voxel-unified jacket with sleeves and trousers; hidden skin culled. Separate islands for real garment layers and accessories. Manual weight fields, linear skinning.'
active(rig)
for area in bpy.context.screen.areas if bpy.context.screen else []:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_distance=3.4
        area.spaces.active.region_3d.view_location=Vector((0,0,1))
        area.spaces.active.region_3d.view_rotation=camera.rotation_euler.to_quaternion()
        area.spaces.active.shading.type='MATERIAL'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'teacher.blend'))
scene.render.filepath=str(OUT/'teacher-preview.png');bpy.ops.render.render(write_still=True)
rig.animation_data.action=walk;scene.frame_set(9)
scene.render.filepath=str(OUT/'teacher-walk-preview.png');bpy.ops.render.render(write_still=True)
rig.animation_data.action=idle;scene.frame_set(1)
camera.location=(.9,-2.5,1.85);data.lens=85;point_at(camera,(0,-.015,1.65))
scene.render.resolution_x=900;scene.render.resolution_y=900
scene.render.filepath=str(OUT/'teacher-face-preview.png');bpy.ops.render.render(write_still=True)
print('TEACHER_BUILD_OK',json.dumps(validation))
