"""Check foot planting and render the saved motion without changing the blend."""
import bpy, json, math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent.parent
scene=bpy.context.scene;rig=bpy.data.objects['Teacher_Rig'];mesh=bpy.data.objects['Teacher_Surface']
metadata=json.loads((ROOT/'build'/'teacher-gait-candidate.json').read_text(encoding='utf8'))
result={}
for name in ('Walk','Run'):
    rig.animation_data.action=bpy.data.actions[name]
    plan=metadata['report'][name];errors=[];contact_heights=[]
    for sample in plan['samples']:
        scene.frame_set(sample['frame']);bpy.context.view_layer.update()
        for side,foot in sample['feet'].items():
            errors.append((rig.pose.bones['foot.'+side].head-Vector(foot['ankle'])).length)
    assert max(errors)<.0005,(name,'IK target error',max(errors))
    # A full gait loop has a duplicated endpoint, including all bone rotations.
    scene.frame_set(1);first={p.name:p.matrix.copy() for p in rig.pose.bones}
    scene.frame_set(int(rig.animation_data.action.frame_range[1]))
    seam=max(abs(p.matrix[i][j]-first[p.name][i][j]) for p in rig.pose.bones for i in range(4) for j in range(4))
    assert seam<.0005,(name,'Loop discontinuity',seam)
    result[name]={'max_ankle_target_error':max(errors),'loop_seam':seam}
rig.animation_data.action=bpy.data.actions['Catch'];scene.frame_set(24)
assert all(rig.pose.bones['hand.'+s].head.y<-.25 for s in ('L','R'))
assert all(abs(rig.pose.bones['hand.'+s].head.x)<.35 for s in ('L','R')), 'Reach spreads sideways'
result['Catch']='hands point toward -Y (front)'
(ROOT/'build'/'teacher-motion-checks.json').write_text(json.dumps(result,indent=2),encoding='utf8')
camera=scene.camera;camera.data.type='ORTHO';camera.data.ortho_scale=2.05
camera.location=(3.5,-.9,1.15);camera.rotation_euler=(Vector((0,0,.97))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.eevee.taa_render_samples=32
scene.render.resolution_x=700;scene.render.resolution_y=850;scene.render.resolution_percentage=100
for name,frame in (('Walk',1),('Walk',16),('Walk',25),('Run',13),('Catch',24)):
    rig.animation_data.action=bpy.data.actions[name];scene.frame_set(frame)
    scene.render.filepath=str(ROOT/'build'/('teacher-'+name.lower()+'-'+str(frame)+'.png'))
    bpy.ops.render.render(write_still=True)
print('TEACHER_MOTION_OK',json.dumps(result),flush=True)
