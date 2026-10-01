import bpy, struct
from pathlib import Path
from mathutils import Matrix
root=Path(__file__).resolve().parent.parent
rig=bpy.data.objects['Teacher_Rig'];scene=bpy.context.scene
names=[b.name for b in rig.data.bones]
print('BONE_ORDER', list(enumerate(names)), flush=True)
basis=Matrix(((1,0,0,0),(0,0,1,0),(0,1,0,0),(0,0,0,1)))
with (root/'assets/models/teacher_runtime.bin').open('rb') as f:
    magic,version,vertices,bones,clips=struct.unpack('<4sIIII',f.read(20));f.seek(vertices*72,1)
    for _ in range(clips):
        name,frames,fps=struct.unpack('<16sIf',f.read(24));name=name.rstrip(b'\0').decode()
        data=f.read(frames*bones*48)
        rig.animation_data.action=bpy.data.actions[name];scene.frame_set(frames);bpy.context.view_layer.update()
        max_error=0
        for i,bone in enumerate(rig.data.bones):
            values=struct.unpack_from('<12f',data,((frames-1)*bones+i)*48)
            expected=basis@(rig.pose.bones[bone.name].matrix@bone.matrix_local.inverted())@basis.inverted()
            max_error=max(max_error,max(abs(values[row*4+col]-expected[row][col]) for row in range(3) for col in range(4)))
            if name=='Catch' and 'hand.' in bone.name:
                mat=Matrix((values[0:4],values[4:8],values[8:12],(0,0,0,1)))
                print('CATCH',i,bone.name,'runtime',list(mat@(basis@bone.head_local)),'blend',list(basis@rig.pose.bones[bone.name].head),flush=True)
        print('CLIP',name,frames,'max_matrix_error',max_error,flush=True)
        assert max_error<1e-5, 'Runtime pose differs from Blender'
