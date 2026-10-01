import bpy, json

out = {
    'objects': [],
    'bones': [],
    'actions': [],
    'materials': [],
}
for o in bpy.data.objects:
    item = {
        'name': o.name,
        'type': o.type,
        'parent': o.parent.name if o.parent else None,
        'hide': bool(o.hide_get()),
        'location': [float(v) for v in o.location],
        'scale': [float(v) for v in o.scale],
        'dimensions': [float(v) for v in o.dimensions],
    }
    if o.type == 'MESH':
        item['verts'] = len(o.data.vertices)
        item['materials'] = [m.name if m else None for m in o.data.materials]
    if o.type == 'ARMATURE':
        item['bones'] = [b.name for b in o.data.bones]
    out['objects'].append(item)
for a in bpy.data.actions:
    out['actions'].append(a.name)
for m in bpy.data.materials:
    out['materials'].append(m.name)
print('TEACHER_INSPECT ' + json.dumps(out, ensure_ascii=False, indent=2))
