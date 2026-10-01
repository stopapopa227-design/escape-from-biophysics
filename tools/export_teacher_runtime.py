"""Export the currently opened manual Teacher_Rig blend into the game runtime format."""
import bpy
import json
import struct
from pathlib import Path
from mathutils import Matrix

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'assets'/'models'
scene=bpy.context.scene
character=bpy.data.objects.get('Teacher_Surface')
rig=bpy.data.objects.get('Teacher_Rig')
if character is None or rig is None or character.type!='MESH' or rig.type!='ARMATURE':
    raise RuntimeError('Expected Teacher_Surface mesh and Teacher_Rig armature in the manual blend.')
if len(rig.data.bones)==0:
    raise RuntimeError('Teacher_Rig has no bones.')
bpy.context.view_layer.update()
character.data.update()


def material_id(mat):
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
    if any(key in name for key in ('coat','tuxedo','wool','lapel','shirt','tie','tailoring')):
        return 5
    return 0


def game_vec(v):
    return (float(v.x),float(v.z),float(v.y))


bone_names=[bone.name for bone in rig.data.bones]
bone_index={name:i for i,name in enumerate(bone_names)}
vertices=[]
for poly in character.data.polygons:
    indices=list(poly.vertices)
    if len(indices)<3:
        continue
    mat=character.data.materials[poly.material_index] if poly.material_index<len(character.data.materials) else None
    surface=material_id(mat)
    for corner in range(1,len(indices)-1):
        for vertex_index in (indices[0],indices[corner],indices[corner+1]):
            source=character.data.vertices[vertex_index]
            influences=[]
            for group in source.groups:
                if group.weight<=0 or group.group>=len(character.vertex_groups):
                    continue
                name=character.vertex_groups[group.group].name
                if name in bone_index:
                    influences.append((float(group.weight),bone_index[name]))
            influences.sort(reverse=True)
            influences=influences[:4]
            if not influences:
                raise RuntimeError(f'Unweighted vertex {vertex_index}')
            total=sum(weight for weight,_ in influences)
            influences=[(weight/total,index) for weight,index in influences]
            while len(influences)<4:
                influences.append((0.0,influences[0][1]))
            p=game_vec(source.co);n=game_vec(source.normal)
            base=tuple(float(max(0.0,min(1.0,x))) for x in (mat.diffuse_color[:3] if mat else (.7,.7,.7)))
            vertices.append((p,n,base,p,tuple(influences),surface))

if len(vertices)>1000000:
    raise RuntimeError(f'Teacher mesh has {len(vertices)} triangle corners; the game accepts at most 1000000. Simplify the mesh before exporting.')

clips=[]
for name in ('Idle','Walk','Run','Catch'):
    action=bpy.data.actions.get(name)
    if action is None and name=='Run':
        continue
    if action is None:
        raise RuntimeError(f'Missing action {name}')
    frames=max(1,int(round(action.frame_range[1])))
    clips.append((name,action,frames))

basis=Matrix(((1,0,0,0),(0,0,1,0),(0,1,0,0),(0,0,0,1)))
inverse_basis=basis.inverted()
original_action=rig.animation_data.action if rig.animation_data else None
sampled_clips=[]
for name,action,frames in clips:
    rig.animation_data_create();rig.animation_data.action=action
    sampled=[]
    for frame in range(1,frames+1):
        scene.frame_set(frame);bpy.context.view_layer.update()
        for bone_name in bone_names:
            pose=rig.pose.bones[bone_name]
            matrix=basis@(pose.matrix@rig.data.bones[bone_name].matrix_local.inverted())@inverse_basis
            sampled.append(tuple(float(matrix[row][col]) for row in range(3) for col in range(4)))
    sampled_clips.append((name,frames,sampled))

runtime_path=OUT/'teacher_runtime.bin'
with runtime_path.open('wb') as out:
    out.write(struct.pack('<4sIIII',b'TCH1',1,len(vertices),len(bone_names),len(sampled_clips)))
    for p,n,color,tex,influences,surface in vertices:
        ids=tuple(index for _,index in influences)
        weights=tuple(weight for weight,_ in influences)
        out.write(struct.pack('<3f3f3f3f4B4fI',*p,*n,*color,*tex,*ids,*weights,surface))
    for name,frames,sampled in sampled_clips:
        out.write(struct.pack('<16sIf',name.encode('ascii'),frames,scene.render.fps))
        for matrix in sampled:
            out.write(struct.pack('<12f',*matrix))

# Keep the portable GLB in sync with the manual blend as well.
rig_hidden=rig.hide_get();rig.hide_set(False)
for selected in list(bpy.context.selected_objects):
    selected.select_set(False)
character.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=character
existing_tracks=[(track,track.mute,track.name) for track in rig.animation_data.nla_tracks]
for track,_,name in existing_tracks:
    track.mute=True;track.name='Backup | '+name
temporary_tracks=[]
for name,action,_ in clips:
    track=rig.animation_data.nla_tracks.new();track.name=name
    track.strips.new(name,1,action);track.mute=False;temporary_tracks.append(track)
rig.animation_data.action=None
bpy.ops.export_scene.gltf(filepath=str(OUT/'teacher.glb'),export_format='GLB',use_selection=True,
    export_animations=True,export_nla_strips=True,export_animation_mode='ACTIONS',
    export_skins=True,export_yup=True)
for track in temporary_tracks:
    rig.animation_data.nla_tracks.remove(track)
for track,mute,name in existing_tracks:
    track.name=name;track.mute=mute
rig.animation_data.action=original_action
rig.hide_set(rig_hidden)
scene.frame_set(1)

validation_path=OUT/'teacher-validation.json'
try:
    validation=json.loads(validation_path.read_text(encoding='utf8'))
except Exception:
    validation={}
validation.update({'vertices':len(character.data.vertices),'polygons':len(character.data.polygons),
                   'bones':len(bone_names),'runtime_asset':'teacher_runtime.bin',
                   'runtime_vertices':len(vertices),'runtime_clips':len(sampled_clips),
                   'clips':[name for name,_,_ in sampled_clips],
                   'source':'manual teacher.blend'})
validation_path.write_text(json.dumps(validation,indent=2),encoding='utf8')
print('TEACHER_RUNTIME_EXPORT_OK',json.dumps({'vertices':len(character.data.vertices),'triangles':len(vertices)//3,'bones':len(bone_names),'runtime_vertices':len(vertices),'clips':[name for name,_,_ in sampled_clips]}))
