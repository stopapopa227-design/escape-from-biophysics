"""Edit the manual teacher.blend in place, preserving its rig and animation.

Run in Blender with the saved asset open and -- --apply. A backup is made once.
"""
import bpy
import bmesh
import hashlib
import json
import math
import shutil
import sys
from pathlib import Path
from mathutils import Quaternion, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'assets' / 'models' / 'teacher.blend'
BACKUP = SOURCE.with_name('teacher-before-arm-adjustments.blend')
REVISION = 'slimmer_arms_inward_palms_no_watch_v1'


def smoothstep(lo, hi, x):
    t = max(0.0, min(1.0, (x - lo) / (hi - lo)))
    return t * t * (3.0 - 2.0 * t)


if '--apply' not in sys.argv:
    raise RuntimeError('Pass -- --apply to change the saved manual model.')
opened = Path(bpy.data.filepath).resolve()
if opened not in (SOURCE.resolve(), BACKUP.resolve()):
    raise RuntimeError('Open teacher.blend or its arm-edit backup before running this edit.')
character = bpy.data.objects['Teacher_Surface']
rig = bpy.data.objects['Teacher_Rig']
if character.get('arm_refinement') == REVISION:
    raise RuntimeError('This arm refinement is already applied; refusing to shrink it twice.')
if character.data.shape_keys:
    raise RuntimeError('This edit requires a mesh without shape keys.')
if BACKUP.exists() and opened != BACKUP.resolve():
    if hashlib.sha256(BACKUP.read_bytes()).digest() != hashlib.sha256(SOURCE.read_bytes()).digest():
        raise RuntimeError('The backup belongs to a different source; preserve it before a fresh edit.')
elif not BACKUP.exists():
    shutil.copy2(SOURCE, BACKUP)

mesh = character.data
group_names = {g.index: g.name for g in character.vertex_groups}
watch_materials = {i for i, m in enumerate(mesh.materials)
                   if 'watch' in m.name.lower() or 'badge' in m.name.lower()}
# The joined watch consists of five islands. Material alone is insufficient:
# the shoes and glasses share those materials, but have different bone weights.
watch_faces = [p for p in mesh.polygons if p.material_index in watch_materials
               and all(any(group_names[g.group] == 'hand.L' and g.weight > .99
                           for g in mesh.vertices[i].groups) for i in p.vertices)]
watch_vertices = {i for p in watch_faces for i in p.vertices}
if not watch_vertices:
    raise RuntimeError('No watch geometry found; refusing an ambiguous edit.')
wrist = rig.data.bones['hand.L'].head_local
if any((mesh.vertices[i].co - wrist).length > .13 for i in watch_vertices):
    raise RuntimeError('Watch selection includes geometry away from the wrist.')
if any(any(i in watch_vertices for i in p.vertices) and p not in watch_faces
       for p in mesh.polygons if p.material_index not in watch_materials):
    raise RuntimeError('Watch geometry unexpectedly shares vertices with the character.')
bm = bmesh.new()
bm.from_mesh(mesh)
bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.verts[i] for i in watch_vertices], context='VERTS')
bm.to_mesh(mesh)
bm.free()

# Rotate the exposed skin and its details. The cuff/sleeve surrounds this
# junction and does not need to be twisted along with the wrist.
skin_materials = {i for i, m in enumerate(mesh.materials)
                  if 'skin' in m.name.lower() or 'age folds' in m.name.lower()}
skin_vertices = {i for p in mesh.polygons if p.material_index in skin_materials for i in p.vertices}

axes = {}
for side in ('L', 'R'):
    for part, reduction in (('upper_arm', .10), ('forearm', .10), ('hand', .06)):
        name = part + '.' + side
        bone = rig.data.bones[name]
        axes[name] = (bone.head_local.copy(),
                      (bone.tail_local - bone.head_local).normalized(), reduction)

changed = 0
for v in mesh.vertices:
    weights = {group_names[g.group]: g.weight for g in v.groups}
    arm_weights = {name: w for name, w in weights.items() if name in axes and w > 0}
    if not arm_weights:
        continue
    original = v.co.copy()
    offset = Vector((0, 0, 0))
    # Reduce cross sections, not bone lengths. Existing shoulder/elbow weights
    # blend the reduction into the unedited torso and through the elbow joint.
    for name, w in arm_weights.items():
        origin, axis, reduction = axes[name]
        radial = original - origin
        radial -= axis * radial.dot(axis)
        offset -= radial * (reduction * w)
    p = original + offset
    for side, sign in (('L', 1), ('R', -1)):
        distal_weight = weights.get('forearm.' + side, 0) + weights.get('hand.' + side, 0)
        if distal_weight <= 0 or v.index not in skin_vertices:
            continue
        origin, axis, _ = axes['hand.' + side]
        longitudinal = (original - origin).dot(axis)
        blend = smoothstep(.005, .050, longitudinal) * min(1.0, distal_weight)
        # Nails face -Y in the source, so palms face +Y. Opposite axial turns
        # put both palms inward and thumbs forward. Blend inside the cuff;
        # hand and nail islands move together.
        rotation = Quaternion(axis, -sign * math.pi * .5 * blend)
        p = origin + rotation @ (p - origin)
    if not all(math.isfinite(c) for c in p):
        raise RuntimeError('Non-finite arm coordinates.')
    v.co = p
    changed += int((p - original).length > 1e-7)

# The manual edit left some tiny nail islands outside the finger surface.
# Fit these details to their own hand now that its side is exposed in the pose.
bm = bmesh.new()
bm.from_mesh(mesh)
bm.verts.ensure_lookup_table()
for side in ('L', 'R'):
    hand_group = character.vertex_groups['hand.' + side].index
    eligible = {v.index for v in mesh.vertices if v.index in skin_vertices
                and any(g.group == hand_group and g.weight > .99 for g in v.groups)}
    remaining = set(eligible)
    islands = []
    while remaining:
        seed = remaining.pop()
        component = {seed}
        queue = [seed]
        while queue:
            for edge in bm.verts[queue.pop()].link_edges:
                for v in edge.verts:
                    if v.index in remaining:
                        remaining.remove(v.index)
                        component.add(v.index)
                        queue.append(v.index)
        islands.append(component)
    details = [island for island in islands if 12 <= len(island) <= 220]
    detail_ids = set().union(*details) if details else set()
    surface_faces = [list(p.vertices) for p in mesh.polygons
                     if p.material_index in skin_materials
                     and any(i in eligible for i in p.vertices)
                     and not any(i in detail_ids for i in p.vertices)]
    bvh = BVHTree.FromPolygons([v.co for v in mesh.vertices], surface_faces)
    for island in details:
        center = sum((mesh.vertices[i].co for i in island), Vector()) / len(island)
        point, normal, _, distance = bvh.find_nearest(center)
        if point is not None and .001 < distance < .018:
            offset = point + normal * .0004 - center
            for i in island:
                mesh.vertices[i].co += offset
bm.free()
mesh.update()
character['arm_refinement'] = REVISION
character['arm_refinement_notes'] = 'Arms 10% slimmer; hands 6% slimmer; palms inward; watch removed.'
# Keep Blender's previous manual-save backup as well as our explicit copy.
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
print('TEACHER_ARM_EDIT_OK', json.dumps({
    'removed_watch_vertices': len(watch_vertices),
    'removed_watch_faces': len(watch_faces),
    'changed_arm_vertices': changed,
    'vertices': len(mesh.vertices),
    'backup': str(BACKUP),
}))
