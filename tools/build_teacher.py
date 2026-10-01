"""Blender 4.0: editable, weighted professor asset. Run with --factory-startup -b -P.

Creates a new scene; saves only assets/models/teacher.blend and derived assets.
Coordinates: metres, Z up, character faces -Y. No external textures or add-ons.
"""
import bpy
import bmesh
import math
import json
import struct
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


def wool_nodes(mat, color, rough):
    """Build a scale-aware procedural wool graph for the jacket and trousers."""
    tree=mat.node_tree;nodes=tree.nodes;links=tree.links
    bsdf=nodes.get('Principled BSDF')
    coords=nodes.new('ShaderNodeTexCoord');coords.name='Wool | generated coordinates'
    mapping=nodes.new('ShaderNodeMapping');mapping.name='Wool | micro scale';mapping.inputs['Scale'].default_value=(1.0,1.0,1.0)
    macro=nodes.new('ShaderNodeTexNoise');macro.name='Wool | broad fibre variation';macro.inputs['Scale'].default_value=5.5;macro.inputs['Detail'].default_value=5.0;macro.inputs['Roughness'].default_value=.78
    fine=nodes.new('ShaderNodeTexNoise');fine.name='Wool | short fibres';fine.inputs['Scale'].default_value=185.0;fine.inputs['Detail'].default_value=2.0;fine.inputs['Roughness'].default_value=.72;fine.inputs['Distortion'].default_value=.18
    voronoi=nodes.new('ShaderNodeTexVoronoi');voronoi.name='Wool | fibre cells';voronoi.distance='EUCLIDEAN';voronoi.feature='DISTANCE_TO_EDGE';voronoi.inputs['Scale'].default_value=330.0
    wave=nodes.new('ShaderNodeTexWave');wave.name='Wool | twill geometry';wave.wave_type='BANDS';wave.bands_direction='X';wave.inputs['Scale'].default_value=235.0;wave.inputs['Distortion'].default_value=7.0;wave.inputs['Detail'].default_value=3.0;wave.inputs['Detail Scale'].default_value=2.0
    folds=nodes.new('ShaderNodeTexNoise');folds.name='Wool | broad cloth folds';folds.inputs['Scale'].default_value=8.5;folds.inputs['Detail'].default_value=5.0;folds.inputs['Roughness'].default_value=.82;folds.inputs['Distortion'].default_value=.55
    fold_wave=nodes.new('ShaderNodeTexWave');fold_wave.name='Wool | fold ridges';fold_wave.wave_type='BANDS';fold_wave.bands_direction='Z';fold_wave.inputs['Scale'].default_value=15.0;fold_wave.inputs['Distortion'].default_value=8.0;fold_wave.inputs['Detail'].default_value=4.0;fold_wave.inputs['Detail Scale'].default_value=2.2
    palette=nodes.new('ShaderNodeMixRGB');palette.name='Wool | charcoal colour breakup';palette.blend_type='MIX';palette.inputs[1].default_value=(*tuple(max(0.0,c*.76) for c in color),1);palette.inputs[2].default_value=(*tuple(min(1.0,c*1.27+.012) for c in color),1)
    fibre_mix=nodes.new('ShaderNodeMixRGB');fibre_mix.name='Wool | fibre albedo';fibre_mix.blend_type='MULTIPLY';fibre_mix.inputs[0].default_value=.22;fibre_mix.inputs[2].default_value=(.76,.78,.82,1)
    height_mul=nodes.new('ShaderNodeMath');height_mul.name='Wool | geometric relief';height_mul.operation='MULTIPLY';height_mul.inputs[1].default_value=.72
    height_add=nodes.new('ShaderNodeMath');height_add.name='Wool | twill relief';height_add.operation='ADD';height_add.inputs[1].default_value=.18
    fold_mix=nodes.new('ShaderNodeMixRGB');fold_mix.name='Wool | fold mask';fold_mix.blend_type='MULTIPLY';fold_mix.inputs[0].default_value=.72
    fold_strength=nodes.new('ShaderNodeMath');fold_strength.name='Wool | fold depth';fold_strength.operation='MULTIPLY';fold_strength.inputs[1].default_value=.32
    height_final=nodes.new('ShaderNodeMath');height_final.name='Wool | weave plus folds';height_final.operation='ADD'
    bump=nodes.new('ShaderNodeBump');bump.name='Wool | raised weave';bump.inputs['Strength'].default_value=.24;bump.inputs['Distance'].default_value=.008
    links.new(coords.outputs['Generated'],mapping.inputs['Vector'])
    for node in (macro,fine,voronoi,wave,folds,fold_wave):links.new(mapping.outputs['Vector'],node.inputs['Vector'])
    links.new(macro.outputs['Fac'],palette.inputs[0]);links.new(palette.outputs['Color'],fibre_mix.inputs[1]);links.new(fine.outputs['Fac'],fibre_mix.inputs[2])
    links.new(fine.outputs['Fac'],height_mul.inputs[0]);links.new(height_mul.outputs[0],height_add.inputs[0]);links.new(voronoi.outputs['Distance'],height_add.inputs[1])
    links.new(folds.outputs['Fac'],fold_mix.inputs[1]);links.new(fold_wave.outputs['Color'],fold_mix.inputs[2]);links.new(fold_mix.outputs['Color'],fold_strength.inputs[0])
    links.new(height_add.outputs[0],height_final.inputs[0]);links.new(fold_strength.outputs[0],height_final.inputs[1]);links.new(height_final.outputs[0],bump.inputs['Height']);links.new(bump.outputs['Normal'],bsdf.inputs['Normal'])
    links.new(fibre_mix.outputs['Color'],bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value=max(.62,rough);bsdf.inputs['Sheen Weight'].default_value=.08;bsdf.inputs['Sheen Tint'].default_value=(.35,.35,.35,1)


def material(name, color, rough=.65, metal=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = rough
    bsdf.inputs['Metallic'].default_value = metal
    if any(key in name.lower() for key in ('wool','tuxedo','lapel')):
        wool_nodes(m,color,rough)
    return m


skin = material('Skin | warm neutral', (.52, .31, .205), .58)
coat = material('Tuxedo | grey wool', (.155, .17, .19), .76)
lapel = material('Lapels | subtly darker grey wool', (.137, .151, .17), .76)
seam = material('Tailoring | dark stitching', (.105, .12, .14), .85)
shirt = material('Shirt | ivory cotton', (.78, .80, .78), .83)
trousers = material('Tuxedo trousers | grey wool', (.13, .145, .166), .76)
leather = material('Shoes and watch strap | dark brown leather', (.016, .012, .010), .28)
sole = material('Shoe sole', (.022, .021, .023), .8)
tie = material('Necktie | black silk', (.015, .019, .026), .34)
steel = material('Glasses and watch | brushed metal', (.30, .29, .27), .24, .82)
hair = material('Hair | silver grey', (.48, .47, .445), .8)
white = material('Eyes | warm white', (.72, .69, .63), .25)
iris = material('Eyes | hazel iris', (.115, .16, .12), .25)
pupil = material('Eyes | pupil', (.009, .012, .014), .18)
lips = material('Lips', (.32, .125, .09), .7)
crease = material('Subtle age folds', (.445, .26, .175), .72)
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
    extension=.005 if name in ('Finger','Thumb') else (.012 if name=='Palm' else .026)
    start=a-axis*extension;end=b+axis*extension
    verts=[];faces=[];segments=32;rings=12
    for j in range(rings):
        t=j/(rings-1)
        # Match adjoining cross-sections at elbows and knees, without a pinched waist.
        profiles={'Jacket upper sleeve':(1,.82),'Jacket lower sleeve':(.98,.85),
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
    if 'lapel' in name:
        original=[Vector(p) for p in points];points=[]
        n=len(original)
        for i in range(n):
            a,b,c,d=(original[(i-1)%n],original[i],original[(i+1)%n],original[(i+2)%n])
            for j in range(4):
                t=j/4
                points.append((b*2+(c-a)*t+(a*2-b*5+c*4-d)*t*t+(-a+b*3-c*3+d)*t*t*t)*.5)
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
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(mesh);bm.free()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);mesh.materials.append(mat)
    for p in mesh.polygons:p.use_smooth=True
    assign_surface(o)
    return o


# Rest pose landmarks. Arms in a relaxed A pose for painting shoulder weights.
landmarks = {}
for side, s in [('L', 1), ('R', -1)]:
    landmarks[side] = dict(shoulder=(s*.235, 0, 1.385), elbow=(s*.40, -.015, 1.20),
                           wrist=(s*.51, -.03, .98), hand=(s*.548, -.035, .875),
                           hip=(s*.112, 0, .91), knee=(s*.112, -.022, .52),
                           ankle=(s*.112, 0, .13), toe=(s*.112, -.18, .075))

# Skin is a genuinely connected surface, not merely a collection of overlapping limbs.
raw = [uv('Skin torso', (0, .005, 1.19), (.211, .121, .33), skin),
       uv('Pelvis', (0, .015, .91), (.19, .115, .18), skin),
       uv('Neck', (0, .005, 1.515), (.073, .067, .13), skin)]
def head_point(q):
    z=1.737+q.z*.168
    jaw=1-.05*math.exp(-((z-1.61)/.043)**2)
    x=q.x*.132*jaw
    y=q.y*.119
    front=max(0,-q.y)**5
    def bump(cx,cz,rx,rz):return math.exp(-((x-cx)/rx)**2-((z-cz)/rz)**2)
    # Anatomical planes: brow ridge, bridge/tip, malar pads, jowls and chin.
    y-=front*(.031*bump(0,1.739,.016,.044)+.043*bump(0,1.699,.022,.019)
              +.010*bump(0,1.642,.058,.026)+.014*bump(0,1.610,.064,.024)
              +.013*(bump(-.069,1.710,.033,.029)+bump(.069,1.710,.033,.029))
              +.012*(bump(-.069,1.639,.038,.034)+bump(.069,1.639,.038,.034))
              +.011*(bump(-.05,1.785,.041,.018)+bump(.05,1.785,.041,.018)))
    y+=front*(.020*(bump(-.050,1.756,.032,.020)+bump(.050,1.756,.032,.020))
              +.006*(bump(-.047,1.671,.016,.035)+bump(.047,1.671,.016,.035)))
    return Vector((x,y,z))

head = uv('Age-sculpted head',(0,0,0),(1,1,1),skin,seg=112,rings=80)
for v in head.data.vertices:v.co=head_point(v.co.copy())
raw.append(head)
for side, d in landmarks.items():
    raw += [link_ellipsoid('Upper arm skin', d['shoulder'], d['elbow'], .061, .066, skin),
            link_ellipsoid('Forearm skin', d['elbow'], d['wrist'], .048, .05, skin),
            link_ellipsoid('Palm', d['wrist'], d['hand'], .038, .023, skin),
            link_ellipsoid('Thigh skin', d['hip'], d['knee'], .077, .078, skin),
            link_ellipsoid('Calf skin', d['knee'], d['ankle'], .056, .06, skin)]
    s = 1 if side == 'L' else -1
    raw.append(uv('Ear', (s*.134, .005, 1.735), (.023, .020, .046), skin))
    # Four visible fingers and an opposed thumb, merged into the palm.
    for i in range(4):
        x = s*(.525+i*.015)
        raw.append(link_ellipsoid('Finger', (x, -.04, .895), (x+s*.015, -.049, .833+abs(i-1.5)*.008), .009, .009, skin))
    raw.append(link_ellipsoid('Thumb', (s*.52, -.04, .931), (s*.502, -.07, .889), .012, .013, skin))
body = fuse(raw, 'Teacher | continuous skin', .004, 2)
# Omit occluded skin under opaque clothing, as in a game character mesh.
# This prevents the hidden torso from protruding through animated shoulders.
bm=bmesh.new();bm.from_mesh(body.data)
hidden=[]
for face in bm.faces:
    p=face.calc_center_median()
    visible_head=p.z>1.49 and abs(p.x)<.17
    visible_hand=p.z<1.025 and abs(p.x)>.43
    if not (visible_head or visible_hand):hidden.append(face)
bmesh.ops.delete(bm,geom=hidden,context='FACES')
bm.to_mesh(body.data);bm.free()

# Jacket and sleeves form another continuous surface, with a tailored waist.
def torso_mesh():
    rows = [(.83,.204,.126),(.88,.224,.14),(1.04,.222,.162),(1.16,.222,.158),
            (1.30,.235,.147),(1.40,.248,.12),(1.46,.177,.087),(1.49,.09,.071)]
    profile=rows;rows=[]
    for i in range(len(profile)-1):
        a=Vector(profile[max(0,i-1)]);b=Vector(profile[i]);c=Vector(profile[i+1]);d=Vector(profile[min(len(profile)-1,i+2)])
        for j in range(5):
            t=j/5
            q=(b*2+(c-a)*t+(a*2-b*5+c*4-d)*t*t+(-a+b*3-c*3+d)*t*t*t)*.5
            q.x=b.x*(1-t)+c.x*t
            rows.append(tuple(q))
    rows.append(profile[-1])
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
    shells += [link_ellipsoid('Jacket upper sleeve',d['shoulder'],d['elbow'],.071,.074,coat),
               link_ellipsoid('Jacket lower sleeve',d['elbow'],cuff,.060,.061,coat)]
jacket=fuse(shells,'Teacher | continuous jacket and sleeves',.008,9)
# Tailored shoulders descend from the neck instead of forming raised sleeve caps.
for v in jacket.data.vertices:
    if abs(v.co.x)>.16 and v.co.z>1.38:
        v.co.z=min(v.co.z,1.49-.43*(abs(v.co.x)-.12))
active(jacket)
relax=jacket.modifiers.new('Relax shoulder line','SMOOTH');relax.factor=.5;relax.iterations=5
bpy.ops.object.modifier_apply(modifier=relax.name)

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
        elif (ax>.20 and z>1.23) or (ax>.30 and z>.78):
            d=landmarks[side]
            p=Vector((ax,y,z));el=Vector((.40,-.015,1.20));wr=Vector((.51,-.03,.98))
            t=smoothstep(-.085,.085,(p-el).dot((wr-el).normalized()))
            h=1-smoothstep(.93,1.02,z)
            weights={f'upper_arm.{side}':(1-t)*(1-h),f'forearm.{side}':t*(1-h),f'hand.{side}':h}
            shoulder=smoothstep(.20,.35,ax)
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
patch('Formal shirt front',[(-.060,-.084,1.487),(.060,-.084,1.487),(.044,-.16,1.158),(-.044,-.16,1.158)],shirt,'chest')
def coat_front(x,z,lift=.006):
    p=coat_bvh.ray_cast(Vector((x,-1,z)),Vector((0,1,0)),2)[0]
    return (x,p.y-lift,z) if p else (x,-.13,z)

for side,s in [('L',1),('R',-1)]:
    patch('Satin shawl lapel '+side,[(s*.061,-.09,1.481),(s*.105,-.09,1.457),
          (s*.15,-.11,1.42),(s*.166,-.12,1.36),(s*.15,-.14,1.295),
          (s*.102,-.15,1.208),(s*.018,-.16,1.107),(s*.037,-.16,1.30),(s*.036,-.13,1.42)],lapel,'chest')
    patch('Shirt collar '+side,[(s*.008,-.08,1.495),(s*.060,-.081,1.477),
          (s*.039,-.13,1.427),(s*.005,-.13,1.456)],shirt,'chest')
    curve('Jetted pocket '+side,[coat_front(s*(.085+i*.006),1.015+i*.0005) for i in range(15)],.0022,lapel,'spine')
    d=landmarks[side]
    uv('Shirt cuff '+side,Vector(d['wrist']).lerp(Vector(d['elbow']),.08),(.049,.044,.027),shirt,'forearm.'+side)
    oxford_shoe(side,s)
    # A subtle satin stripe on the outside of the trousers.
    curve('Trouser side stripe '+side,[(s*.194,.004,.83),(s*.196,-.005,.70),(s*.188,-.015,.56)],.0018,lapel,'thigh.'+side)

# Pleated shirt bib with a black necktie fitted over the chest.
for x in (-.033,-.022,-.011,.011,.022,.033):
    curve('Shirt pleat',[coat_front(x,1.19+i*.012,.0058) for i in range(19)],.00055,paper,'chest')
for z in (1.38,1.32,1.26,1.20):
    uv('Shirt stud',coat_front(0,z,.010),(.0028,.002,.0028),steel,'chest',16,12)
uv('Necktie knot',coat_front(0,1.454,.019),(.014,.009,.019),tie,'chest',32,20)
patch('Silk necktie',[(-.009,-.13,1.44),(.009,-.13,1.44),(.026,-.16,1.19),(0,-.17,1.155),(-.026,-.16,1.19)],tie,'chest')
uv('Single satin-covered fastening',coat_front(.012,1.11,.006),(.007,.0035,.007),lapel,'spine',24,16)
curve('Chest pocket',[coat_front(-.155+i*.004,1.33,.005) for i in range(17)],.0017,seam,'chest')
patch('Folded pocket square',[(-.15,-.10,1.332),(-.128,-.13,1.362),(-.119,-.13,1.342),(-.099,-.13,1.356),(-.09,-.13,1.332)],shirt,'chest')

# Facial details follow the sculpted head, including age folds and eyelids.
face_bvh=BVHTree.FromPolygons([v.co for v in body.data.vertices],[p.vertices[:] for p in body.data.polygons])
def face_front(x,z,lift=.001):
    p=face_bvh.ray_cast(Vector((x,-1,z)),Vector((0,1,0)),2)[0]
    return (x,p.y-lift,z) if p else (x,-.10,z)
for side,s in [('L',1),('R',-1)]:
    uv('Ear concha '+side,(s*.145,-.009,1.733),(.009,.005,.024),lips,'head')
    curve('Ear helix '+side,[(s*(.139+.012*math.cos(i*math.tau/40)),-.012,1.737+.034*math.sin(i*math.tau/40)) for i in range(40)],.0034,skin,'head',True)
    ey=face_front(s*.05,1.755,.001)[1]
    uv('Eye '+side,(s*.05,ey,1.755),(.024,.011,.011),white,'head',40,24)
    uv('Iris '+side,(s*.05,ey-.0105,1.754),(.0072,.0017,.0072),iris,'head')
    uv('Pupil '+side,(s*.05,ey-.012,1.754),(.0033,.001,.0033),pupil,'head')
    for upper in (True,False):
        points=[]
        for i in range(25):
            a=math.pi*i/24
            points.append((s*.05+.024*math.cos(a),ey-.005-.004*math.sin(a),1.755+(.010 if upper else -.008)*math.sin(a)))
        curve('Soft eyelid '+side,points,.0035,skin,'head')
    for k in (0,1):
        curve('Under-eye age fold '+side,[face_front(s*(.024+i*.0027),1.733-k*.005-.002*math.sin(i*math.pi/20),.0008) for i in range(21)],.00045,crease,'head')
    curve('Expressive silver brow '+side,[face_front(s*(.02+i*.0028),1.787+.005*math.sin(i*math.pi/24)-i*.00023,.003) for i in range(25)],.0055,hair,'head')
    for i in range(22):
        x=s*(.023+i*.0028);z=1.789+.005*math.sin(i*math.pi/24)-i*.00023
        curve('Brow hair',[face_front(x,z,.007),face_front(x+s*.003,z+.003,.007)],.00065,hair,'head')
    curve('Glasses rim '+side,[(s*.05+.041*math.cos(i*math.tau/64),-.157,1.755+.026*math.sin(i*math.tau/64)) for i in range(64)],.0022,steel,'head',True)
    curve('Glasses temple '+side,[(s*.091,-.156,1.76),(s*.13,-.065,1.77),(s*.14,.020,1.77),(s*.137,.035,1.744)],.0022,steel,'head')
    uv('Nostril '+side,face_front(s*.013,1.685,.001),(.004,.002,.0025),lips,'head',16,12)
    curve('Nasolabial fold '+side,[face_front(s*(.025+i*.0012),1.690-i*.0022,.0006) for i in range(20)],.00035,crease,'head')
    for k in range(3):
        curve('Crow foot '+side,[face_front(s*(.078+i*.0014),1.748+k*.003-i*.0004,.0005) for i in range(14)],.00028,crease,'head')
curve('Glasses bridge',[(-.01,-.158,1.759),(0,-.166,1.768),(.01,-.158,1.759)],.0021,steel,'head')
curve('Mouth line',[face_front(-.032+i*.002,1.653+.003*math.sin(i*math.pi/32),.0015) for i in range(33)],.0015,lips,'head')
curve('Lower lip',[face_front(-.026+i*.002,1.649+.001*math.sin(i*math.pi/26),.002) for i in range(27)],.0024,skin,'head')
for k in range(3):
    curve('Forehead crease',[face_front(-.078+i*.004,1.810+k*.017+.003*math.sin(i*math.pi/39),.00035) for i in range(40)],.00028,crease,'head')

# A visible silver horseshoe around the temples; receding top remains bare.
verts=[];faces=[]
for j in range(17):
    v=.70+j*.055
    for i in range(81):
        a=-.30+i*(math.pi+.60)/80
        q=Vector((math.sin(v)*math.cos(a),math.sin(v)*math.sin(a),math.cos(v)))
        p=head_point(q)+q*.0018;verts.append(p)
for j in range(16):
    for i in range(80):faces.append((j*81+i,j*81+i+1,(j+1)*81+i+1,(j+1)*81+i))
m=bpy.data.meshes.new('Receding hair mass');m.from_pydata(verts,[],faces);m.update()
o=bpy.data.objects.new('Silver hair silhouette',m);scene.collection.objects.link(o);m.materials.append(hair)
for poly in m.polygons:poly.use_smooth=True
weight(o,'head')
for i in range(800):
    a=-.29+(i*.61803398875%1)*(math.pi+.58);v=.72+(i%113)/113*.86
    points=[]
    for j in range(5):
        u=a+j*.017;v2=v+j*.010
        q=Vector((math.sin(v2)*math.cos(u),math.sin(v2)*math.sin(u),math.cos(v2)))
        points.append(head_point(q)+q*(.0025+.001*math.sin(j*math.pi/4)))
    curve('Combed silver strand',points,.00065,hair,'head')

for side,sign in [('L',1),('R',-1)]:
    for i in range(4):
        x=sign*(.525+i*.015+.010)
        z=.846+abs(i-1.5)*.008
        uv('Fingernail '+side,(x,-.061,z),(.0047,.001,.008),skin,'hand.'+side,16,12)
        curve('Knuckle crease '+side,[(x-.004,-.061,z+.017),(x+.004,-.061,z+.017)],.00045,crease,'hand.'+side)

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
def proportions(p):
    p=Vector(p)
    if p.z>1.49:
        t=min(1,(p.z-1.49)/.12)
        p.z-=.052*t
        p.y-=.009*t
    return p
for v in character.data.vertices:v.co=proportions(v.co)
arm=bpy.data.armatures.new('Teacher_Skeleton')
rig=bpy.data.objects.new('Teacher_Rig',arm)
scene.collection.objects.link(rig)
active(rig)
bpy.ops.object.mode_set(mode='EDIT')


def bone(name, head, tail, parent=None):
    b=arm.edit_bones.new(name);b.head=proportions(head);b.tail=proportions(tail)
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
            # A compact, slightly asymmetric gait: pelvis leads the step,
            # shoulders counter-rotate, and the torso settles between steps.
            rig.pose.bones['pelvis'].location.x=.006*math.sin(phase*2)
            rig.pose.bones['pelvis'].location.y=.010*(1-math.cos(phase*2))
            rotation('pelvis',(0,0,1),.022*math.sin(phase*2))
            rotation('spine',(0,0,1),.045*math.sin(phase))
            rotation('chest',(0,0,1),-.068*math.sin(phase))
            rotation('chest',(1,0,0),.020*math.sin(phase*2))
        elif kind=='idle':
            rotation('chest',(1,0,0),.007*math.sin(phase))
            rotation('head',(0,0,1),.035*math.sin(phase))
        else:
            amount=math.sin(min(1,(frame-1)/(frames-1)*1.4)*math.pi/2)
            rotation('chest',(1,0,0),.10*amount)
        for side,s in [('L',1),('R',-1)]:
            ph=phase+(0 if side=='L' else math.pi)
            stride=.275*math.sin(ph) if kind=='walk' else 0
            lift=max(0,-math.sin(ph)) if kind=='walk' else 0
            knee=.075+.40*(lift**1.22) if kind=='walk' else .045
            rotation('thigh.'+side,(1,0,0),stride)
            rotation('shin.'+side,(1,0,0),-knee)
            rotation('foot.'+side,(1,0,0),-.72*stride+.62*knee)
            # Arms swing opposite the stepping leg and relax toward the body.
            arm_phase=ph+math.pi
            arm_swing=math.sin(arm_phase) if kind=='walk' else 0
            rotation('upper_arm.'+side,(0,1,0),s*.40)
            base=rig.pose.bones['upper_arm.'+side].rotation_quaternion.copy()
            rotation('upper_arm.'+side,(1,0,0),.20*arm_swing if kind=='walk' else (.95*amount if kind=='reach' else 0))
            rig.pose.bones['upper_arm.'+side].rotation_quaternion=base@rig.pose.bones['upper_arm.'+side].rotation_quaternion
            elbow_bend=.14+.11*max(0,arm_swing) if kind=='walk' else (.16 if kind!='reach' else .25*amount)
            rotation('forearm.'+side,(1,0,0),elbow_bend)
            rotation('hand.'+side,(1,0,0),-.035*arm_swing if kind=='walk' else 0)
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

# The game has a deliberately small OpenGL loader, so keep a deterministic
# binary beside the GLB.  It contains the same one-piece mesh, four normalized
# influences per vertex, and sampled bone matrices for the three actions.
def runtime_material(mat):
    name=(mat.name if mat else '').lower()
    if 'skin' in name or 'lip' in name or 'crease' in name:
        return 6
    if 'eye' in name or 'iris' in name or 'pupil' in name:
        return 13
    if 'hair' in name:
        return 8
    if 'steel' in name or 'watch' in name or 'metal' in name:
        return 7
    if 'shoe' in name or 'leather' in name:
        return 9
    if 'paper' in name or 'badge' in name:
        return 15
    if 'coat' in name or 'tuxedo' in name or 'wool' in name or 'lapel' in name or 'shirt' in name or 'tie' in name or 'tailoring' in name:
        return 5
    return 0


def runtime_vec(v):
    # Blender is Z-up and the game is Y-up.  Keep the model facing -Z.
    return (float(v.x), float(v.z), float(v.y))


def write_runtime_asset():
    bone_names=[b.name for b in arm.bones]
    bone_index={name:i for i,name in enumerate(bone_names)}
    vertices=[]
    for poly in character.data.polygons:
        if len(poly.vertices)<3:
            continue
        mat=character.data.materials[poly.material_index] if poly.material_index<len(character.data.materials) else None
        material_id=runtime_material(mat)
        indices=list(poly.vertices)
        for corner in range(1,len(indices)-1):
            for vi in (indices[0],indices[corner],indices[corner+1]):
                source=character.data.vertices[vi]
                influences=[]
                for group in source.groups:
                    if group.weight<=0:
                        continue
                    name=character.vertex_groups[group.group].name
                    if name in bone_index:
                        influences.append((float(group.weight),bone_index[name]))
                influences.sort(reverse=True)
                influences=influences[:4]
                if not influences:
                    raise RuntimeError('Runtime export found an unweighted vertex')
                total=sum(weight for weight,_ in influences)
                influences=[(weight/total,index) for weight,index in influences]
                while len(influences)<4:
                    influences.append((0.0,influences[0][1]))
                p=runtime_vec(source.co); n=runtime_vec(source.normal)
                color=tuple(float(max(0.0,min(1.0,x))) for x in (mat.diffuse_color[:3] if mat else (0.7,0.7,0.7)))
                vertices.append((p,n,color,tuple(influences)))

    clips=[('Idle',idle,61),('Walk',walk,33),('Catch',reach,25)]
    matrices=[]
    basis=__import__('mathutils').Matrix(((1,0,0,0),(0,0,1,0),(0,1,0,0),(0,0,0,1)))
    inverse_basis=basis.inverted()
    for _,action,frames in clips:
        rig.animation_data.action=action
        sampled=[]
        for frame in range(1,frames+1):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            for name in bone_names:
                pose=rig.pose.bones[name]
                matrix=basis @ (pose.matrix @ arm.bones[name].matrix_local.inverted()) @ inverse_basis
                sampled.append(tuple(float(matrix[row][col]) for row in range(3) for col in range(4)))
        matrices.append(sampled)

    path=OUT/'teacher_runtime.bin'
    with path.open('wb') as out:
        out.write(struct.pack('<4sIIII',b'TCH1',1,len(vertices),len(bone_names),len(clips)))
        cursor=0
        for poly in character.data.polygons:
            if len(poly.vertices)<3:
                continue
            mat=character.data.materials[poly.material_index] if poly.material_index<len(character.data.materials) else None
            material_id=runtime_material(mat)
            indices=list(poly.vertices)
            for corner in range(1,len(indices)-1):
                for vi in (indices[0],indices[corner],indices[corner+1]):
                    p,n,color,influences=vertices[cursor];cursor+=1
                    ids=tuple(index for _,index in influences);weights=tuple(weight for weight,_ in influences)
                    out.write(struct.pack('<3f3f3f3f4B4fI',*p,*n,*color,*p,*ids,*weights,material_id))
        for (name,_,frames),sampled in zip(clips,matrices):
            out.write(struct.pack('<16sIf',name.encode('ascii'),frames,scene.render.fps))
            for matrix in sampled:
                out.write(struct.pack('<12f',*matrix))
    rig.animation_data.action=idle
    scene.frame_set(1)
    return path, len(vertices), len(matrices)


runtime_path,runtime_vertices,runtime_clip_count=write_runtime_asset()

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
            'front':'-Y','up':'Z','units':'metres','blender':bpy.app.version_string,
            'runtime_asset':'teacher_runtime.bin','runtime_vertices':runtime_vertices,
            'runtime_clips':runtime_clip_count}
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


# Seamless studio cyclorama, avoiding a distracting horizon behind the face.
profile=[(-20,0),(2,0)]
for i in range(1,25):
    a=i*math.pi/48;profile.append((2+2*math.sin(a),2-2*math.cos(a)))
profile.append((4,12))
verts=[(x,y,z) for y,z in profile for x in (-30,30)]
faces=[(i*2,i*2+1,i*2+3,i*2+2) for i in range(len(profile)-1)]
m=bpy.data.meshes.new('Studio cyclorama');m.from_pydata(verts,[],faces);m.update()
floor=bpy.data.objects.new('Studio cyclorama',m);studio.objects.link(floor)
m.materials.append(material('Studio slate',(.043,.057,.073),.9))
for p in m.polygons:p.use_smooth=True
world=bpy.data.worlds.new('Studio world');scene.world=world;world.use_nodes=True
world.node_tree.nodes['Background'].inputs[0].default_value=(.11,.14,.19,1)
world.node_tree.nodes['Background'].inputs[1].default_value=.3


def point_at(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


for name,pos,power,size in [('Key',(-3,-4,5),450,4),('Fill',(3,-2,3),230,3),('Rim',(1,2,4),550,3)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new(name,data);studio.objects.link(o);o.location=pos;point_at(o,(0,0,1))
data=bpy.data.cameras.new('Teacher preview camera');camera=bpy.data.objects.new('Teacher preview camera',data)
studio.objects.link(camera);scene.camera=camera;camera.location=(2.35,-5,2.15);data.lens=78;point_at(camera,(0,0,.99))
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
camera.location=(.7,-2.5,1.83);data.lens=85;point_at(camera,(0,-.02,1.60))
scene.render.resolution_x=900;scene.render.resolution_y=900
scene.render.filepath=str(OUT/'teacher-face-preview.png');bpy.ops.render.render(write_still=True)
print('TEACHER_BUILD_OK',json.dumps(validation))
