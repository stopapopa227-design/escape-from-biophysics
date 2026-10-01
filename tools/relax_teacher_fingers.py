"""Bend only the existing hand vertices; preserve manual mesh, weights and actions."""
import bpy, json, math, shutil, heapq
from pathlib import Path
from datetime import datetime
from mathutils import Vector

root=Path(__file__).resolve().parent.parent
obj=bpy.data.objects['Teacher_Surface'];rig=bpy.data.objects['Teacher_Rig'];scene=bpy.context.scene
if obj.get('relaxed_fingers_version'):
    raise RuntimeError('Hands are already relaxed; do not accumulate this deformation.')
frames=json.loads(obj['reference_palm_frames'])
backup=root/'assets/models/teacher-before-relaxed-fingers.blend'
if not backup.exists():shutil.copy2(bpy.data.filepath,backup)
mesh=obj.data
skin_slots={i for i,m in enumerate(mesh.materials) if 'skin' in m.name.lower()}
eligible={i for p in mesh.polygons if p.material_index in skin_slots for i in p.vertices}
specs=[(.027,.146,.90),(.008,.157,1.08),(-.0115,.149,1.18),(-.029,.131,1.25)]
def smooth(t):
    t=max(0,min(1,t));return t*t*(3-2*t)
def bend(x,y,z,finger=0):
    # Smooth knuckle curvature, progressively stronger toward the little finger.
    # Cross sections rotate with the centreline, including their fitted nails.
    if y>.084:
        fx,end,angle=specs[finger]
        base=.084;length=end-base;s=y-base;t=s/length
        def theta(u):return angle*(.62*smooth(u/.65)+.38*smooth((u-.42)/.58))
        cy=cz=0
        for k in range(32):
            a=theta(t*(k+.5)/32);cy+=math.cos(a)*s/32;cz+=math.sin(a)*s/32
        a=theta(t)
        return Vector((x-(x-fx)*.32*smooth(t),base+cy-z*math.sin(a),cz+z*math.cos(a)))
    # Draw the thumb inward and slightly across the palm, retaining the web.
    w=smooth((x-.035)/.034)*(1-smooth((y-.065)/.019))
    a=.42*w;dx=x-.032;dy=y-.028
    return Vector((.032+math.cos(a)*dx-math.sin(a)*dy-.004*w,
                   .028+math.sin(a)*dx+math.cos(a)*dy,z+.010*w))

changed=0;left_local={};max_shift=0
for side in ('L','R'):
    frame=frames[side];origin=Vector(frame['origin']);u=Vector(frame['thumb']);v=Vector(frame['down']);n=Vector(frame['normal'])
    group=obj.vertex_groups['hand.'+side].index
    local_points={}
    for vertex in mesh.vertices:
        if vertex.index not in eligible or not any(g.group==group and g.weight>.99 for g in vertex.groups):continue
        q=vertex.co-origin;x,y,z=q.dot(u),q.dot(v),q.dot(n)
        local_points[vertex.index]=Vector((x,y,z))
    # Label connected distal finger surfaces (and their nail islands), then
    # propagate toward the knuckles. X-only selection can split a finger edge.
    adjacency={i:[] for i,p in local_points.items() if p.y>.084}
    for edge in mesh.edges:
        a,b=edge.vertices
        if a in adjacency and b in adjacency:adjacency[a].append(b);adjacency[b].append(a)
    remaining={i for i in adjacency if local_points[i].y>.108};labels={};queue=[];distances={}
    while remaining:
        seed=remaining.pop();component=[seed];stack=[seed]
        while stack:
            for j in adjacency[stack.pop()]:
                if j in remaining:remaining.remove(j);component.append(j);stack.append(j)
        x=sum(local_points[i].x for i in component)/len(component)
        label=min(range(4),key=lambda k:abs(x-specs[k][0]))
        for i in component:labels[i]=label;distances[i]=0;heapq.heappush(queue,(0,i,label))
    while queue:
        distance,i,label=heapq.heappop(queue)
        if distance>distances[i]:continue
        for j in adjacency[i]:
            candidate_distance=distance+(local_points[i]-local_points[j]).length
            if candidate_distance<distances.get(j,float('inf')):
                distances[j]=candidate_distance;labels[j]=label;heapq.heappush(queue,(candidate_distance,j,label))
    for index,p in local_points.items():
        vertex=mesh.vertices[index]
        local=bend(*p,labels.get(index,0));updated=origin+u*local.x+v*local.y+n*local.z
        shift=(updated-vertex.co).length
        if shift>1e-8:changed+=1
        max_shift=max(max_shift,shift);vertex.co=updated
        if side=='L':left_local[vertex.index]=local
assert changed>1000 and .015<max_shift<.08,(changed,max_shift)
mesh.update();obj['relaxed_fingers_version']='gentle_progressive_curl_v1'
obj['relaxed_fingers_notes']='Existing skin and nail vertices curved toward palm; thumb brought inward; no new geometry or weights.'
rig.animation_data.action=bpy.data.actions['Idle'];scene.frame_set(1);bpy.context.view_layer.update()
candidate=root/'build'/('teacher-relaxed-fingers-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.blend')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(candidate))
(root/'build/teacher-relaxed-fingers.json').write_text(json.dumps({'path':str(candidate),'changed_vertices':changed,'max_displacement':max_shift},indent=2),encoding='utf8')

camera=scene.camera;camera.data.type='ORTHO';camera.data.ortho_scale=2.05
camera.location=(0,-4,1.13);camera.rotation_euler=(Vector((0,0,1.04))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.eevee.taa_render_samples=48
scene.render.resolution_x=800;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(root/'build/teacher-relaxed-full.png');bpy.ops.render.render(write_still=True)
indices=list(left_local);lookup={i:k for k,i in enumerate(indices)}
faces=[p for p in mesh.polygons if all(i in lookup for i in p.vertices)]
detail=bpy.data.meshes.new('Relaxed hand preview')
detail.from_pydata([(left_local[i].x,-left_local[i].z,1-left_local[i].y) for i in indices],[],[[lookup[i] for i in p.vertices] for p in faces])
for m in mesh.materials:detail.materials.append(m)
for p,source in zip(detail.polygons,faces):p.material_index=source.material_index;p.use_smooth=True
preview=bpy.data.objects.new('Hand detail preview',detail);scene.collection.objects.link(preview);obj.hide_render=True
camera.data.ortho_scale=.23;camera.location=(.17,-.40,1.04)
camera.rotation_euler=(Vector((.012,0,.923))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(root/'build/teacher-relaxed-hand.png');bpy.ops.render.render(write_still=True)
print('RELAXED_FINGERS_READY',str(candidate),changed,max_shift,flush=True)
