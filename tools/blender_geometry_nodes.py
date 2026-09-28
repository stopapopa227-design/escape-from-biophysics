"""Optional Blender 4.2+ authoring scene. Run from Blender's Scripting workspace.

Creates a NEW scene, leaves existing scenes intact, and saves nothing automatically.
The C++ game does not need Blender. Export its exact professor mesh first with
build/EscapeFromBiophysics.exe --export-model (working directory: build).
"""
from pathlib import Path
import math
import bpy

ROOT = Path(__file__).resolve().parent.parent
scene = bpy.data.scenes.new('MSU - Geometry Nodes authoring')
bpy.context.window.scene = scene


def material(name, asset, tint=(1, 1, 1, 1)):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    shader = nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = tint
    shader.inputs['Roughness'].default_value = 0.55
    if asset:
        coord = nodes.new('ShaderNodeTexCoord')
        coord.location = (-850, 0)
        for kind, socket, y in [('Diffuse', 'Base Color', 200), ('Rough', 'Roughness', -50)]:
            image = nodes.new('ShaderNodeTexImage')
            image.image = bpy.data.images.load(str(ROOT / 'assets' / 'materials' / f'{asset}-{kind}.png'), check_existing=True)
            if kind != 'Diffuse':
                image.image.colorspace_settings.name = 'Non-Color'
            image.projection = 'BOX'
            image.projection_blend = 0.15
            image.location = (-550, y)
            links.new(coord.outputs['Object'], image.inputs['Vector'])
            links.new(image.outputs['Color'], shader.inputs[socket])
    return mat


plaster = material('Scanned beige plaster', 'beige_wall_001')
floor = material('Scanned terrazzo', 'terrazzo_tiles')
wood = material('Varnished classroom doors', 'wood_table_001')
stone = material('Warm limestone', None, (0.55, 0.49, 0.38, 1))

host_mesh = bpy.data.meshes.new('Procedural corridor host')
host = bpy.data.objects.new('Faculty corridor - edit Geometry Nodes', host_mesh)
scene.collection.objects.link(host)
tree = bpy.data.node_groups.new('MSU corridor | primitives - materials - repeat - join', 'GeometryNodeTree')
tree.interface.new_socket(name='Geometry', in_out='OUTPUT', socket_type='NodeSocketGeometry')
count = tree.interface.new_socket(name='Bays', in_out='INPUT', socket_type='NodeSocketInt')
count.default_value = 8
count.min_value = 1
count.max_value = 30
nodes, links = tree.nodes, tree.links
inputs = nodes.new('NodeGroupInput')
inputs.location = (-950, 600)
output = nodes.new('NodeGroupOutput')
output.location = (900, 0)
join = nodes.new('GeometryNodeJoinGeometry')
join.location = (660, 0)
links.new(join.outputs['Geometry'], output.inputs['Geometry'])
line = nodes.new('GeometryNodeMeshLine')
line.mode = 'OFFSET'
line.inputs['Start Location'].default_value = (0, 0, 0)
line.inputs['Offset'].default_value = (0, 3.2, 0)
line.location = (-700, 600)
links.new(inputs.outputs['Bays'], line.inputs['Count'])


def repeated_box(label, size, position, mat, row):
    primitive = nodes.new('GeometryNodeMeshCube')
    primitive.label = label
    primitive.inputs['Size'].default_value = size
    primitive.location = (-930, row)
    transform = nodes.new('GeometryNodeTransform')
    transform.inputs['Translation'].default_value = position
    transform.location = (-700, row)
    links.new(primitive.outputs['Mesh'], transform.inputs['Geometry'])
    surface = nodes.new('GeometryNodeSetMaterial')
    surface.inputs['Material'].default_value = mat
    surface.location = (-470, row)
    links.new(transform.outputs['Geometry'], surface.inputs['Geometry'])
    instance = nodes.new('GeometryNodeInstanceOnPoints')
    instance.location = (-220, row)
    links.new(line.outputs['Mesh'], instance.inputs['Points'])
    links.new(surface.outputs['Geometry'], instance.inputs['Instance'])
    realize = nodes.new('GeometryNodeRealizeInstances')
    realize.location = (20, row)
    links.new(instance.outputs['Instances'], realize.inputs['Geometry'])
    links.new(realize.outputs['Geometry'], join.inputs['Geometry'])


repeated_box('Terrazzo floor bay', (3.2, 3.2, 0.10), (0, 0, -0.05), floor, 300)
repeated_box('High plaster ceiling', (3.2, 3.2, 0.12), (0, 0, 3.86), plaster, 0)
for side in (-1, 1):
    row = -300 if side == -1 else -1500
    repeated_box('Continuous plaster wall', (0.18, 3.2, 3.8), (side * 1.69, 0, 1.9), plaster, row)
    repeated_box('Wood dado', (0.035, 3.2, 2.81), (side * 1.59, 0, 1.405), wood, row - 260)
    repeated_box('Classroom door', (0.06, 1.3, 2.64), (side * 1.56, 0, 1.32), wood, row - 520)
    for tier in range(3):
        repeated_box('Cornice profile', (0.08 + tier * 0.04, 3.2, 0.055),
                     (side * 1.57, 0, 3.61 + tier * 0.065), plaster, -2900 - (tier + (side + 1) * 2) * 230)
modifier = host.modifiers.new('Procedural academic corridor', 'NODES')
modifier.node_group = tree

model = ROOT / 'build' / 'professor.obj'
if model.exists():
    bpy.ops.wm.obj_import(filepath=str(model), forward_axis='Y', up_axis='Z')
    for obj in list(bpy.context.selected_objects):
        obj.location = (0, 3.6, 0)
        obj.rotation_euler.z = math.pi
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
else:
    print('Professor OBJ missing. Run --export-model in build to include the game model.')

for index in range(8):
    light = bpy.data.lights.new(f'Ceiling luminaire {index}', 'AREA')
    light.energy = 180
    light.shape = 'RECTANGLE'
    light.size = 0.45
    light.size_y = 1.4
    obj = bpy.data.objects.new(light.name, light)
    scene.collection.objects.link(obj)
    obj.location = (0, index * 3.2, 3.65)
camera = bpy.data.cameras.new('Corridor camera')
obj = bpy.data.objects.new(camera.name, camera)
scene.collection.objects.link(obj)
obj.location = (0, -1.3, 1.66)
obj.rotation_euler = (math.pi / 2, 0, 0)
camera.lens = 24
scene.camera = obj
scene.render.engine = 'CYCLES'
scene.cycles.samples = 64
scene.render.resolution_x = 1600
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
bpy.context.view_layer.objects.active = host
host.select_set(True)
print('Created editable corridor Geometry Nodes. Save the new scene explicitly if desired.')
