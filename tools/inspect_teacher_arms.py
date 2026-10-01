"""Inspect the saved manual mesh and render an arm-edit reference; never saves it."""
import bpy
import json
import sys
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parent.parent
obj = bpy.data.objects['Teacher_Surface']
rig = bpy.data.objects['Teacher_Rig']
scene = bpy.context.scene
groups = {g.index: g.name for g in obj.vertex_groups}
for side in ('L', 'R'):
    for part in ('upper_arm', 'forearm', 'hand'):
        name = part + '.' + side
        points = [v.co for v in obj.data.vertices
                  if any(groups[g.group] == name and g.weight > .8 for g in v.groups)]
        print('ARM_REGION', name, len(points),
              [[round(f(p[i] for p in points), 5) for i in range(3)] for f in (min, max)])
watch_mats = {i for i, m in enumerate(obj.data.materials)
              if 'watch' in m.name.lower() or 'badge' in m.name.lower()}
faces = [p for p in obj.data.polygons if p.material_index in watch_mats
         and all(any(groups[g.group] == 'hand.L' and g.weight > .99
                     for g in obj.data.vertices[i].groups) for i in p.vertices)]
ids = {i for p in faces for i in p.vertices}
print('WATCH_CANDIDATES', len(faces), len(ids),
      [[round(f(obj.data.vertices[j].co[i] for j in ids), 5) for i in range(3)] for f in (min, max)] if ids else [])
print('SHAPE_KEYS', obj.data.shape_keys)
print('MODIFIERS', [(m.name, m.type) for m in obj.modifiers])
rig.animation_data.action = bpy.data.actions['Idle']
for track in rig.animation_data.nla_tracks:
    track.mute = True
scene.frame_set(1)
camera = scene.camera
camera.location = (0, -4, 1.12)
camera.rotation_euler = (Vector((0, 0, 1.05)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 2.02
scene.render.engine = 'BLENDER_EEVEE'
scene.eevee.taa_render_samples = 32
scene.render.resolution_x = 800
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
label = 'after' if '--after' in sys.argv else 'before'
scene.render.filepath = str(root / 'build' / ('teacher-arms-' + label + '.png'))
bpy.ops.render.render(write_still=True)
if '--after' in sys.argv:
    camera.location = (0, -4, .99)
    camera.rotation_euler = (Vector((0, 0, .98)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = .86
    scene.render.resolution_x = 1100
    scene.render.resolution_y = 850
    scene.render.filepath = str(root / 'build' / 'teacher-hands-after.png')
    bpy.ops.render.render(write_still=True)
