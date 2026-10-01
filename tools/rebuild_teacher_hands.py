"""Replace only the hands/cuffs of the manual asset with reference-shaped meshes.

Run with teacher.blend open. Saves a candidate in build/ for visual review.
The existing skeleton, actions, body and sleeves are retained.
"""
import bpy
import bmesh
import json
import math
import shutil
import sys
from datetime import datetime
from pathlib import Path
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets' / 'models'
BACKUP = OUT / 'teacher-before-reference-hands.blend'
scene = bpy.context.scene
character = bpy.data.objects['Teacher_Surface']
rig = bpy.data.objects['Teacher_Rig']
if character.get('reference_hands_version'):
    raise RuntimeError('Already rebuilt; start from the saved pre-reference backup for revisions.')
if not BACKUP.exists():
    shutil.copy2(bpy.data.filepath, BACKUP)
if bpy.context.object and bpy.context.object.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

skin = bpy.data.materials['Skin | warm neutral']
shirt = bpy.data.materials['Shirt | ivory cotton']
coat = bpy.data.materials['Tuxedo | grey wool']
nail = skin.copy()
nail.name = 'Skin | subtle natural nails'
nail.diffuse_color = tuple(min(1.0, c * 1.06 + .025) for c in skin.diffuse_color[:3]) + (1,)
nail.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = nail.diffuse_color
nail.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = .5


def active(obj):
    for o in list(bpy.context.selected_objects):
        o.select_set(False)
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def mesh_object(name, verts, faces, material):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.000001)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(material)
    for p in mesh.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    return obj


def rings_object(name, rows, material, segments=48, power=1.0):
    verts = []
    for x, y, z, rx, rz in rows:
        for i in range(segments):
            a = math.tau * i / segments
            c, s = math.cos(a), math.sin(a)
            c = math.copysign(abs(c) ** power, c)
            s = math.copysign(abs(s) ** power, s)
            verts.append((x + rx * c, y, z + rz * s))
    faces = []
    for j in range(len(rows) - 1):
        for i in range(segments):
            k = j * segments + i
            ni = j * segments + (i + 1) % segments
            faces.append((k, ni, ni + segments, k + segments))
    faces += [tuple(reversed(range(segments))), tuple((len(rows)-1)*segments+i for i in range(segments))]
    return mesh_object(name, verts, faces, material)


def ellipsoid(name, center, radii, material):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=20)
    obj = bpy.context.object
    obj.name = name
    for v in obj.data.vertices:
        v.co = Vector(center) + Vector(tuple(v.co[i] * radii[i] for i in range(3)))
    obj.data.materials.append(material)
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def join(objects, name):
    active(objects[0])
    for obj in objects:
        obj.select_set(True)
    bpy.ops.object.join()
    objects[0].name = name
    return objects[0]


def bezier(points, t):
    a, b, c, d = map(Vector, points)
    return a * ((1-t)**3) + b * (3*t*(1-t)**2) + c * (3*t*t*(1-t)) + d * (t**3)


finger_specs = [
    (.027, .071, .146, .0089, .004),
    (.008, .074, .157, .0093, .001),
    (-.0115, .072, .149, .0088, -.002),
    (-.029, .065, .131, .0075, -.004),
]


def finger(name, x, start, end, radius, spread):
    rows = []
    for j in range(41):
        t = j / 40
        r = radius * (1 - .16*t)
        if t > .89:
            r *= math.sqrt(max(.000001, 1 - ((t-.89)/.11)**2))
        rows.append((x + spread*t, start+(end-start)*t,
                     .001 + .006*t*t, r, r*.87))
    return rings_object(name, rows, skin, 32)


def make_hand():
    # Local +Y runs from wrist to fingertips, +Z is unequivocally the PALM,
    # +X is the thumb side. Build one surface with gently separated digits.
    profile = [(-.036, .028, .019), (-.020, .027, .018),
               (0, .0255, .0165), (.016, .028, .0165),
               (.035, .034, .0175), (.055, .038, .0155),
               (.072, .037, .0105), (.081, .031, .008),
               (.086, .021, .004), (.088, .002, .001)]
    rows = []
    for j in range(len(profile)-1):
        for k in range(5):
            t = k/5
            a, b = profile[j], profile[j+1]
            y, rx, rz = [a[i]*(1-t)+b[i]*t for i in range(3)]
            rows.append((0, y, 0, rx, rz))
    y, rx, rz = profile[-1]
    rows.append((0, y, 0, rx, rz))
    pieces = [rings_object('Reference palm and wrist', rows, skin, power=.88)]
    for i, spec in enumerate(finger_specs):
        pieces.append(finger('Rounded finger %d' % i, *spec))
        x,start,end,radius,spread=spec
        pieces.append(ellipsoid('Soft knuckle web %d'%i, (x,start+.009,.0005),
                                (radius*1.12,.017,radius*1.16),skin))
    pieces.append(ellipsoid('Soft thumb pad', (.025, .035, .004), (.018, .024, .015), skin))
    points = [(.024, .026, .001), (.040, .028, .003), (.061, .031, .005), (.073, .046, .008)]
    verts, faces = [], []
    segments = 36
    for j in range(33):
        t = j/32
        center = bezier(points, t)
        direction = (bezier(points, min(1, t+.001)) - bezier(points, max(0, t-.001))).normalized()
        u = direction.cross(Vector((0, 0, 1))).normalized()
        v = u.cross(direction).normalized()
        radius = .0115*(1-.22*t)
        if t > .82:
            radius *= math.sqrt(max(.000001, 1-((t-.82)/.18)**2))
        for i in range(segments):
            a = math.tau*i/segments
            verts.append(center + radius*(u*math.cos(a)+v*.86*math.sin(a)))
    for j in range(32):
        for i in range(segments):
            k=j*segments+i; ni=j*segments+(i+1)%segments
            faces.append((k, ni, ni+segments, k+segments))
    faces += [tuple(reversed(range(segments))), tuple(32*segments+i for i in range(segments))]
    pieces.append(mesh_object('Opposed rounded thumb', verts, faces, skin))
    hand = join(pieces, 'Reference hand surface')
    active(hand)
    mod = hand.modifiers.new('Continuous palm and five fingers', 'REMESH')
    mod.mode = 'VOXEL'; mod.voxel_size = .00125; mod.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    mod = hand.modifiers.new('Soft finger webs', 'SMOOTH')
    mod.factor = .55; mod.iterations = 4
    bpy.ops.object.modifier_apply(modifier=mod.name)
    # Shallow actual creases in the palm, not raised dark strips on its back.
    paths = [[(.027,.062), (.014,.064), (-.009,.069), (-.027,.070)],
             [(.019,.043), (.009,.047), (-.008,.052), (-.027,.053)],
             [(.025,.022), (.019,.030), (.013,.040), (.012,.051)]]
    def distance_to_path(x, y, path):
        p=Vector((x,y)); result=1.0
        for a,b in zip(path, path[1:]):
            a,b=Vector(a),Vector(b); d=b-a
            t=max(0,min(1,(p-a).dot(d)/d.length_squared))
            result=min(result,(p-(a+d*t)).length)
        return result
    for vertex in hand.data.vertices:
        x,y,z=vertex.co
        if z > .007 and .015 < y < .08:
            depth=max(.0004*math.exp(-(distance_to_path(x,y,p)/.0011)**2) for p in paths)
            vertex.co.z-=depth
        if z > .004:
            for fx, start, end, radius, spread in finger_specs:
                t=(y-start)/(end-start)
                if .25<t<.8 and abs(x-fx-spread*t)<radius*.86:
                    vertex.co.z-=.00035*math.exp(-((t-.51)/.025)**2)
    hand.data.update()
    # Keep the new hands inside the client's one-million-corner mesh budget.
    # The dense voxel surface is only a sculpting intermediate.
    active(hand)
    simplify=hand.modifiers.new('Game hand topology', 'DECIMATE')
    simplify.ratio=.40
    simplify.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=simplify.name)
    bvh=BVHTree.FromPolygons([v.co for v in hand.data.vertices],[list(p.vertices) for p in hand.data.polygons])
    nails=[]
    for i,(x,start,end,radius,spread) in enumerate(finger_specs):
        t=.77
        cx=x+spread*t; cy=start+(end-start)*t
        # Every point follows the existing dorsal surface; no floating ovals.
        verts=[(cx,cy,bvh.ray_cast(Vector((cx,cy,-.1)),Vector((0,0,1)))[0].z-.0002)]
        for k in range(40):
            a=math.tau*k/40
            px=cx+radius*.60*math.cos(a); py=cy+.008*math.sin(a)
            hit=bvh.ray_cast(Vector((px,py,-.1)),Vector((0,0,1)))[0]
            verts.append((px,py,(hit.z if hit else .002)-.00015))
        faces=[(0,1+(k+1)%40,1+k) for k in range(40)]
        nails.append(mesh_object('Fitted nail %d'%i,verts,faces,nail))
    return hand, nails


def make_cuff():
    # A short hollow shirt cuff, with rounded lips, aligned to the wrist.
    profile=[(-.032,.040,.032),(-.026,.040,.032),(-.010,.035,.027),
             (-.003,.0335,.025),(-.001,.032,.024),
             (-.001,.028,.020),(-.004,.028,.020),(-.030,.029,.021)]
    rows=[(0,y,0,rx,rz) for y,rx,rz in profile]
    rows.append(rows[0])
    return rings_object('Ivory shirt cuff',rows,shirt,64)


# Remove the old hands, nails and cuffs, but keep the user's jacket and sleeves.
mesh = character.data
groups={g.index:g.name for g in character.vertex_groups}
hand_materials={i for i,m in enumerate(mesh.materials)
                if any(key in m.name.lower() for key in ('skin','age folds','shirt'))}
eligible={i for p in mesh.polygons if p.material_index in hand_materials for i in p.vertices}
remove={v.index for v in mesh.vertices if v.index in eligible and v.co.z<1.12 and abs(v.co.x)>.30
        and any(groups[g.group].startswith(('forearm.','hand.')) and g.weight>0 for g in v.groups)}
if not 5000 < len(remove) < 18000:
    raise RuntimeError('Unexpected old-hand selection size: %d'%len(remove))
bm=bmesh.new(); bm.from_mesh(mesh); bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm,geom=[bm.verts[i] for i in remove],context='VERTS')
bm.to_mesh(mesh); bm.free()

hand, nails = make_hand()
cuff = make_cuff()
local_parts=[hand]+nails+[cuff]
preview_data=[(o.data.copy(), o.name) for o in local_parts]
parts=[]
palms={}
for side, sign in (('L',1),('R',-1)):
    bone=rig.data.bones['hand.'+side]
    origin=bone.head_local.copy()
    down=(bone.tail_local-origin).normalized()
    inward=Vector((-sign,0,0))
    inward=(inward-down*inward.dot(down)).normalized()
    thumb=sign*inward.cross(down).normalized()
    palms[side]={'origin':list(origin),'normal':list(inward),'down':list(down),'thumb':list(thumb)}
    for source in local_parts:
        obj=bpy.data.objects.new(source.name+'.'+side,source.data.copy())
        scene.collection.objects.link(obj)
        for vertex in obj.data.vertices:
            p=vertex.co.copy()
            vertex.co=origin+thumb*p.x+down*p.y+inward*p.z
        # Reflection between hands changes winding, so recompute outward normals.
        bm=bmesh.new(); bm.from_mesh(obj.data)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        if source.name.startswith('Fitted nail') and sum((f.normal for f in bm.faces),Vector()).dot(-inward)<0:
            bmesh.ops.reverse_faces(bm,faces=list(bm.faces))
        bm.to_mesh(obj.data); bm.free()
        hand_group=obj.vertex_groups.new(name='hand.'+side)
        forearm_group=obj.vertex_groups.new(name='forearm.'+side)
        for vertex in obj.data.vertices:
            along=(vertex.co-origin).dot(down)
            t=max(0,min(1,(along+.022)/.032)); t=t*t*(3-2*t)
            if source==cuff: t=0
            if t>0: hand_group.add([vertex.index],t,'REPLACE')
            if t<1: forearm_group.add([vertex.index],1-t,'REPLACE')
        parts.append(obj)
for obj in local_parts:
    bpy.data.objects.remove(obj,do_unlink=True)
active(character)
for obj in parts: obj.select_set(True)
bpy.ops.object.join()
character['reference_hands_version']='rounded_palm_inward_v2'
character['reference_palm_frames']=json.dumps(palms)
character['arm_refinement_notes']='Slim sleeves retained; rebuilt rounded hands with four fingers and thumb, palm creases inward, nails outward, ivory cuffs, no watch.'

# Check the anatomical palm normals after skinning, across the actual clips.
orientation={}
for action_name in ('Idle','Walk','Catch'):
    rig.animation_data.action=bpy.data.actions[action_name]
    for track in rig.animation_data.nla_tracks: track.mute=True
    samples=[]
    for frame in range(1,int(rig.animation_data.action.frame_range[1])+1,4):
        scene.frame_set(frame); bpy.context.view_layer.update()
        for side,sign in (('L',1),('R',-1)):
            bone=rig.data.bones['hand.'+side]
            matrix=rig.pose.bones[bone.name].matrix@bone.matrix_local.inverted()
            normal=(matrix.to_3x3()@Vector(palms[side]['normal'])).normalized()
            toward_midline=Vector((-sign,0,0))
            dot=normal.dot(toward_midline)
            if dot<.75: raise RuntimeError((action_name,frame,side,'Palm faces away from body',dot))
            samples.append(dot)
    orientation[action_name]=round(min(samples),5)
rig.animation_data.action=bpy.data.actions['Idle']; scene.frame_set(1)
character.data.update()
runtime_vertices=sum((len(p.vertices)-2)*3 for p in character.data.polygons)
if runtime_vertices>1000000:
    raise RuntimeError('Hand mesh exceeds the runtime vertex budget: %d'%runtime_vertices)
candidate=ROOT/'build'/('teacher-hands-candidate-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.blend')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(candidate))
(ROOT/'build'/'teacher-hands-candidate.json').write_text(json.dumps({'path':str(candidate),'removed_vertices':len(remove),'vertices':len(character.data.vertices),'runtime_vertices':runtime_vertices,'palm_inward_dot_min':orientation},indent=2),encoding='utf8')
print('TEACHER_REFERENCE_HANDS_OK',str(candidate),orientation,flush=True)
if '--no-preview' in sys.argv:
    bpy.ops.wm.quit_blender()
    sys.exit(0)

# Inspect the actual new geometry both as part of the character and in palm view.
camera=scene.camera
camera.data.type='ORTHO'; camera.data.ortho_scale=2.02
camera.location=(0,-4,1.12)
camera.rotation_euler=(Vector((0,0,1.05))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=48
scene.render.resolution_x=800;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'build'/'teacher-reference-hands-full.png')
bpy.ops.render.render(write_still=True)
character.hide_render=True
preview_objects=[]
for data,name in preview_data:
    obj=bpy.data.objects.new('Detail preview | '+name,data)
    scene.collection.objects.link(obj)
    for vertex in data.vertices:
        x,y,z=vertex.co
        vertex.co=(-x,-z,1.0-y)
    preview_objects.append(obj)
# Only the hand is enlarged for this view; the dimensions in the asset stay in metres.
camera.location=(0,-1, .943)
camera.rotation_euler=(Vector((-.015,0,.943))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.ortho_scale=.245
scene.render.resolution_x=900; scene.render.resolution_y=1000
scene.render.filepath=str(ROOT/'build'/'teacher-reference-palm.png')
bpy.ops.render.render(write_still=True)
