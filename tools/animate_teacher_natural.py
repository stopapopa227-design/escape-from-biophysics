"""Rebake locomotion and reaching on the saved manual character, without mesh edits."""
import bpy
import json
import math
import shutil
from datetime import datetime
from pathlib import Path
from mathutils import Vector, Quaternion, Matrix

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'assets'/'models'
rig=bpy.data.objects['Teacher_Rig']; mesh=bpy.data.objects['Teacher_Surface']; scene=bpy.context.scene
backup=OUT/'teacher-before-natural-gait.blend'
if not backup.exists(): shutil.copy2(bpy.data.filepath,backup)
for track in rig.animation_data.nla_tracks: track.mute=True
original_idle=bpy.data.actions['Idle']
sole={}
for side in ('L','R'):
    group=mesh.vertex_groups['foot.'+side].index
    ankle=rig.data.bones['foot.'+side].head_local
    sole[side]=[v.co-ankle for v in mesh.data.vertices if any(g.group==group and g.weight>.99 for g in v.groups)]

def ease(t):
    t=max(0,min(1,t));return t*t*(3-2*t)

def set_rotation(name, rotations):
    basis=rig.data.bones[name].matrix_local.to_quaternion()
    q=Quaternion()
    for axis,angle in rotations:q=q@Quaternion(Vector(axis),angle)
    rig.pose.bones[name].rotation_quaternion=basis.inverted()@q@basis

def set_world(name,head,rotation):
    rest=rig.data.bones[name]
    rig.pose.bones[name].matrix=Matrix.Translation(head)@rotation.to_matrix().to_4x4()@rest.matrix_local.to_quaternion().to_matrix().to_4x4()
    bpy.context.view_layer.update()

def leg(side,ankle,pitch):
    thigh=rig.data.bones['thigh.'+side]; shin=rig.data.bones['shin.'+side]
    hip=rig.pose.bones['thigh.'+side].head.copy()
    delta=ankle-hip; length=delta.length; axis=delta.normalized()
    a=thigh.length;b=shin.length
    if length>a+b+.0001:raise RuntimeError(('Unreachable ankle',side,length,a+b))
    along=(a*a-b*b+length*length)/(2*length)
    height=math.sqrt(max(0,a*a-along*along))
    front=Vector((0,-1,0));front=(front-axis*front.dot(axis)).normalized()
    knee=hip+axis*along+front*height
    set_world(thigh.name,hip,(thigh.tail_local-thigh.head_local).rotation_difference(knee-hip))
    set_world(shin.name,knee,(shin.tail_local-shin.head_local).rotation_difference(ankle-knee))
    set_world('foot.'+side,ankle,Quaternion(Vector((1,0,0)),pitch))

def foot_target(side,phase,stride,stance,running):
    p=phase%1
    if p<stance:
        y=stride*(p-stance*.5)
        clearance=0
        pitch=-.20*(1-ease(p/.12))+.38*ease((p-(stance-.13))/.13)
    else:
        t=(p-stance)/(1-stance)
        a=stride*stance*.5;b=-a;m=stride*(1-stance)
        y=(2*t**3-3*t*t+1)*a+(t**3-2*t*t+t)*m+(-2*t**3+3*t*t)*b+(t**3-t*t)*m
        clearance=(.15 if running else .082)*math.sin(math.pi*t)**2
        pitch=.38*(1-ease(t))-.20*ease(t)
    rot=Quaternion(Vector((1,0,0)),pitch)
    z=.004-min((rot@v).z for v in sole[side])+clearance
    x=rig.data.bones['foot.'+side].head_local.x
    return Vector((x,y,z)),pitch,p<stance

def new_action(name):
    old=bpy.data.actions.get(name)
    if old:
        old.name=name+' | previous'
        old.use_fake_user=False
        for track in list(rig.animation_data.nla_tracks):
            if any(strip.action==old for strip in track.strips):rig.animation_data.nla_tracks.remove(track)
        if rig.animation_data.action==old:rig.animation_data.action=None
        if old.users==0:bpy.data.actions.remove(old)
    action=bpy.data.actions.new(name);action.use_fake_user=True;rig.animation_data.action=action
    return action

def reset():
    for pb in rig.pose.bones:
        pb.location=(0,0,0);pb.rotation_mode='QUATERNION';pb.rotation_quaternion=(1,0,0,0);pb.scale=(1,1,1)
    # Refresh parent matrices before assigning world-space IK transforms.
    bpy.context.view_layer.update()

def keyframe(frame):
    for pb in rig.pose.bones:
        # Keep quaternion signs continuous through the loop for Blender/glTF.
        path='pose.bones["'+pb.name+'"].rotation_quaternion'
        curves=[rig.animation_data.action.fcurves.find(path,index=i) for i in range(4)]
        if all(curves):
            previous=Quaternion(tuple(c.evaluate(frame-1) for c in curves))
            if previous.dot(pb.rotation_quaternion)<0:pb.rotation_quaternion.negate()
        pb.keyframe_insert('location',frame=frame,group=pb.name)
        pb.keyframe_insert('rotation_quaternion',frame=frame,group=pb.name)

def finish(action,frames):
    for curve in action.fcurves:
        for key in curve.keyframe_points:key.interpolation='LINEAR'
    track=rig.animation_data.nla_tracks.new();track.name=action.name
    track.strips.new(action.name,1,action);track.mute=True
    print('ANIMATION_BAKED',action.name,frames,flush=True)

report={}
for name,stride,stance,frames,running in (('Walk',1.20,.54,61,False),('Run',1.75,.42,49,True)):
    action=new_action(name);samples=[]
    for frame in range(1,frames+1):
        scene.frame_set(frame);reset()
        cycle=(frame-1)/(frames-1);phase=cycle*math.tau
        targets={s:foot_target(s,cycle+(0 if s=='L' else .5),stride,stance,running) for s in ('L','R')}
        sway=(.012 if running else .015)*math.sin(phase)
        pelvis_rot=Quaternion(Vector((0,0,1)),-.032*math.cos(phase))@Quaternion(Vector((0,1,0)),.012*math.sin(phase))
        pelvis=rig.data.bones['pelvis']; hip_anchor=pelvis.head_local.copy()
        limits=[.884+(.026 if running else .008)*math.sin(phase*2-.4)]
        for side,(ankle,_,_) in targets.items():
            hip_offset=pelvis_rot@(rig.data.bones['thigh.'+side].head_local-hip_anchor)
            dx=hip_anchor.x+sway+hip_offset.x-ankle.x;dy=hip_anchor.y+hip_offset.y-ankle.y
            reach=rig.data.bones['thigh.'+side].length+rig.data.bones['shin.'+side].length-.009
            limits.append(ankle.z+math.sqrt(max(.01,reach*reach-dx*dx-dy*dy))-hip_offset.z)
        z=min(limits);z-=math.log(sum(math.exp(-(v-z)*160) for v in limits))/160
        set_world('pelvis',Vector((hip_anchor.x+sway,hip_anchor.y,z)),pelvis_rot)
        set_rotation('spine',[((0,0,1),.022*math.cos(phase)),((1,0,0),.027 if not running else .067)])
        set_rotation('chest',[((0,0,1),.044*math.cos(phase)),((0,1,0),-.012*math.sin(phase))])
        set_rotation('neck',[((1,0,0),-.014 if not running else -.04)])
        set_rotation('head',[((0,0,1),-.025*math.cos(phase)),((1,0,0),.009*math.sin(phase*2-.4))])
        for side,sign in (('L',1),('R',-1)):
            ph=phase+(0 if side=='L' else math.pi)
            swing=(.31 if running else .20)*math.cos(ph-.10)
            set_rotation('upper_arm.'+side,[((0,1,0),sign*.40),((1,0,0),swing)])
            set_rotation('forearm.'+side,[((1,0,0),-(.55 if running else .19)-(.12 if running else .065)*(1-math.cos(ph-.45))*.5)])
            set_rotation('hand.'+side,[((1,0,0),.025*math.sin(ph-.35))])
        bpy.context.view_layer.update()
        for side,(ankle,pitch,contact) in targets.items():leg(side,ankle,pitch)
        keyframe(frame)
        samples.append({'frame':frame,'pelvis_z':round(z,5),'feet':{s:{'ankle':list(p),'contact':contact} for s,(p,_,contact) in targets.items()}})
    finish(action,frames);report[name]={'stride_metres':stride,'stance_fraction':stance,'samples':samples}

# Non-looping reach: negative X bends hanging arms toward Blender front (-Y).
action=new_action('Catch')
for frame in range(1,25):
    scene.frame_set(frame);reset();amount=ease((frame-1)/23)
    set_rotation('chest',[((1,0,0),.075*amount)])
    for side,sign in (('L',1),('R',-1)):
        # Lower the resting A-pose first, then lift in the forward sagittal plane.
        # Reversing this order spreads the hands sideways during the reach.
        set_rotation('upper_arm.'+side,[((1,0,0),-1.20*amount),((0,1,0),sign*(.40+.12*amount))])
        set_rotation('forearm.'+side,[((1,0,0),-.19-.15*amount)])
        set_rotation('hand.'+side,[((1,0,0),.08*amount)])
    bpy.context.view_layer.update();keyframe(frame)
finish(action,24)
scene.frame_set(24);bpy.context.view_layer.update()
for side in ('L','R'):
    assert rig.pose.bones['hand.'+side].head.y<-.25, 'Reach points backward'
rig.animation_data.action=original_idle;scene.frame_set(1)
rig['locomotion']='Planted-foot analytical IK; 1.20 m walk and 1.75 m run stride; counter-rotating shoulders; forward held reach.'
path=ROOT/'build'/('teacher-gait-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.blend')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(path))
(ROOT/'build'/'teacher-gait-candidate.json').write_text(json.dumps({'path':str(path),'clips':list(report),'report':report},indent=2),encoding='utf8')
print('NATURAL_GAIT_READY',str(path),flush=True)
