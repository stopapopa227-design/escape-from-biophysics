import bpy, json
from mathutils import Quaternion
rig=bpy.data.objects.get('Teacher_Rig')
rig.hide_set(False)
out={'bones':{},'actions':{}}
for n in ['upper_arm.L','forearm.L','hand.L','upper_arm.R','forearm.R','hand.R']:
 b=rig.data.bones.get(n); p=rig.pose.bones.get(n)
 out['bones'][n]={
  'head':[round(float(x),5) for x in b.head_local], 'tail':[round(float(x),5) for x in b.tail_local],
  'matrix_local':[[round(float(x),5) for x in row] for row in b.matrix_local],
  'rotation_mode':p.rotation_mode,
  'pose_quat':[round(float(x),5) for x in p.rotation_quaternion],
 }
for a in bpy.data.actions:
 for n in ['hand.L','hand.R']:
  chans=[fc for fc in a.fcurves if fc.data_path.endswith('rotation_quaternion') and f'pose.bones["{n}"]' in fc.data_path]
  vals=[]
  for f in [1, a.frame_range[0], a.frame_range[1]]:
   vals.append([round(float(fc.evaluate(f)),5) for fc in chans])
  out['actions'].setdefault(a.name,{})[n]=vals
print(json.dumps(out,ensure_ascii=False,indent=2))
