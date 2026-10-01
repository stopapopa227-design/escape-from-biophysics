"""Run in Blender with the saved teacher.blend open. Does not modify that file."""
import bpy
import json
import math
import struct
from pathlib import Path
from mathutils import Vector

out=Path(__file__).resolve().parent.parent/'assets'/'models'
scene=bpy.context.scene
rig=bpy.data.objects['Teacher_Rig']
mesh=bpy.data.objects['Teacher_Surface']
assert len(rig.data.bones)==20
assert any(m.type=='ARMATURE' and m.object==rig for m in mesh.modifiers)
for v in mesh.data.vertices:
    assert 1<=len(v.groups)<=4
    assert abs(sum(g.weight for g in v.groups)-1)<1e-4
    assert all(mesh.vertex_groups[g.group].name in rig.data.bones for g in v.groups)
samples={}
palm_frames=json.loads(mesh.get('reference_palm_frames','{}'))
palm_alignment={}
clip_names=('Idle','Walk','Run','Catch') if bpy.data.actions.get('Run') else ('Idle','Walk','Catch')
for name in clip_names:
    action=bpy.data.actions[name]
    rig.animation_data.action=action
    samples[name]=[]
    for f in range(1,int(action.frame_range[1])+1,4):
        scene.frame_set(f)
        bpy.context.view_layer.update()
        evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        assert all(math.isfinite(c) for v in evaluated.data.vertices for c in v.co)
        low=min(v.co.z for v in evaluated.data.vertices)
        assert -.015<low<(.16 if name=='Run' else .025),(name,f,low)
        samples[name].append({'frame':f,'min_z':round(low,5)})
        for side,frame in palm_frames.items():
            bone=rig.data.bones['hand.'+side]
            skin_matrix=rig.pose.bones[bone.name].matrix@bone.matrix_local.inverted()
            palm_normal=(skin_matrix.to_3x3()@Vector(frame['normal'])).normalized()
            alignment=palm_normal.dot(Vector((-1 if side=='L' else 1,0,0)))
            assert alignment>.75,(name,f,side,'Palm faces away from torso',alignment)
            palm_alignment[name]=min(palm_alignment.get(name,1.0),alignment)
data=(out/'teacher.glb').read_bytes()
assert data[:4]==b'glTF'
length=struct.unpack_from('<I',data,12)[0]
gltf=json.loads(data[20:20+length])
assert {a['name'] for a in gltf['animations']}==set(clip_names)
assert len(gltf['skins'])==1 and len(gltf['skins'][0]['joints'])==20
for m in gltf['meshes']:
    for p in m['primitives']:
        assert {'JOINTS_0','WEIGHTS_0'}<=p['attributes'].keys()
report={'saved_blend':'PASS','normalized_weights':'PASS','glb_skin_and_clips':'PASS','poses':samples}
if palm_frames:
    report['palms_inward']='PASS'
    report['minimum_palm_alignment']={k:round(v,5) for k,v in palm_alignment.items()}
(out/'teacher-checks.json').write_text(json.dumps(report,indent=2),encoding='utf8')

# Export a close view of the shoe geometry from the saved asset.
rig.animation_data.action=bpy.data.actions['Idle'];scene.frame_set(1)
camera=scene.camera
camera.location=(.55,-.85,.38)
camera.rotation_euler=(Vector((0,-.06,.10))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.lens=65
scene.render.resolution_x=1000;scene.render.resolution_y=700
scene.render.filepath=str(out/'teacher-shoes-preview.png')
bpy.ops.render.render(write_still=True)
print('TEACHER_VALIDATION_OK',json.dumps({k:len(v) for k,v in samples.items()}))
