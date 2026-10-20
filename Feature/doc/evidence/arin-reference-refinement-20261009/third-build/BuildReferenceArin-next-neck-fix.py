"""Arin v3: repair and rig the supplied volumetric FBX, preserve its UV/detail.

Dedicated background process only. Originals and v2 are never saved or edited.
--preview stops before baking/exports; final work retains the source shader file.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import bmesh
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).parent))
import BuildArin as common

OUT=ROOT/'Art/NPC/Arin/ReferenceRefined'
EVIDENCE=ROOT/'Feature/doc/evidence/arin-reference-refinement-20261009'
INPUT=OUT/'Inputs/FantasyAdventurer_Source.fbx'
SOURCE_SCRIPT_HASHES={name:hashlib.sha256((Path(__file__).parent/name).read_bytes()).hexdigest()
    for name in ('BuildReferenceArin.py','BuildArin.py','ArinSurface.py')}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def normalized_source():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(INPUT),use_anim=False)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    if len(meshes)!=1:
        raise RuntimeError('Expected the inspected single donor mesh')
    obj=meshes[0]
    source_image=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE' and n.image)
    original=OUT/'Inputs/Original_Color.jpg'
    original.write_bytes(bytes(source_image.packed_file.data))
    source_image.filepath='//Inputs/Original_Color.jpg'
    source_image.name='Arin_Donor_OriginalColor'
    points=[obj.matrix_world@v.co for v in obj.data.vertices]
    minz,maxz=min(p.z for p in points),max(p.z for p in points)
    scale=1.7/(maxz-minz)
    # The actual donor front is +X and its T-pose arms span Y.
    for vertex,point in zip(obj.data.vertices,points):
        vertex.co=(point.y*scale,-point.x*scale,(point.z-minz)*scale)
    obj.location=(0,0,0)
    obj.rotation_euler=(0,0,0)
    obj.scale=(1,1,1)
    obj.name='Arin_Adventurer'
    obj.data.name='Arin_DonorRepaired_UVPreserved'
    obj['arin_atlas']='Adventurer'
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=1
    return obj,source_image,{'rotation_z_degrees':-90,'uniform_scale':scale,'height_m':1.7,
        'foot_z_m':0,'front_axis':'-Y','transforms_applied':True}


def repair_surface(obj):
    """Fill only small closed holes; preserve UVs and all face identity surfaces."""
    bm=bmesh.new()
    bm.from_mesh(obj.data)
    before={'vertices':len(bm.verts),'faces':len(bm.faces),'boundary_edges':sum(e.is_boundary for e in bm.edges)}
    loose=[v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm,geom=loose,context='VERTS')
    remaining_vertices=set(bm.verts)
    tiny_overlap=[]
    while remaining_vertices:
        seed=remaining_vertices.pop()
        pending=[seed]
        component=[seed]
        while pending:
            item=pending.pop()
            for edge in item.link_edges:
                neighbor=edge.other_vert(item)
                if neighbor in remaining_vertices:
                    remaining_vertices.remove(neighbor)
                    pending.append(neighbor)
                    component.append(neighbor)
        center=sum((v.co for v in component),Vector())/len(component)
        crown=(center-Vector((-.007,-.119,1.625))).length<.016
        bracer=(center-Vector((.540,.013,1.290))).length<.019
        if len(component)<60 and (crown or bracer):
            tiny_overlap+=component
    if tiny_overlap:
        bmesh.ops.delete(bm,geom=tiny_overlap,context='VERTS')
    remaining={e for e in bm.edges if e.is_boundary}
    groups=[]
    while remaining:
        edge=remaining.pop()
        pending=[edge]
        connected=[edge]
        while pending:
            item=pending.pop()
            for vertex in item.verts:
                for other in vertex.link_edges:
                    if other in remaining:
                        remaining.remove(other)
                        pending.append(other)
                        connected.append(other)
        groups.append(connected)
    uv=bm.loops.layers.uv.active
    repaired=[]
    for edges in groups:
        vertices=set(v for e in edges for v in e.verts)
        center=sum((v.co for v in vertices),Vector())/len(vertices)
        perimeter=sum(e.calc_length() for e in edges)
        closed=all(sum(v in e.verts for e in edges)==2 for v in vertices)
        # Visible crown hole and short hair cracks. Do not close garment openings,
        # fused seams, the face mouth line or intentional layered hems.
        hair=center.z>1.54
        bracer=.515<abs(center.x)<.56 and 1.27<center.z<1.31 and .005<center.y<.025
        boot=abs(center.x)>.19 and .29<center.z<.40
        if not(closed and 3<=len(vertices)<=24 and ((perimeter<.061 and hair) or ((bracer or boot) and perimeter<.065))):
            continue
        samples={v:next((loop[uv].uv.copy() for loop in v.link_loops),None) for v in vertices}
        adjacent={f for e in edges for f in e.link_faces}
        reference=max(adjacent,key=lambda f:f.calc_area())
        anchor=list(reference.loops)[:3]
        a,ab,ac=anchor[0].vert.co,anchor[1].vert.co-anchor[0].vert.co,anchor[2].vert.co-anchor[0].vert.co
        ua,ub,uc=(loop[uv].uv.copy() for loop in anchor)
        determinant=ab.dot(ab)*ac.dot(ac)-ab.dot(ac)**2
        result=bmesh.ops.holes_fill(bm,edges=edges,sides=24)
        faces=result.get('faces',[])
        for face in faces:
            face.smooth=True
            for loop in face.loops:
                if abs(determinant)>1e-14:
                    q=loop.vert.co-a
                    s=(q.dot(ab)*ac.dot(ac)-q.dot(ac)*ab.dot(ac))/determinant
                    t=(q.dot(ac)*ab.dot(ab)-q.dot(ab)*ab.dot(ac))/determinant
                    loop[uv].uv=ua+(ub-ua)*s+(uc-ua)*t
                elif samples.get(loop.vert) is not None:
                    loop[uv].uv=samples[loop.vert]
        repaired.append({'center_m':list(center),'perimeter_m':perimeter,'new_faces':len(faces)})
    # Export uses deterministic triangles. Triangulate only donor n-gons now;
    # the dense quad-dominant authored surface and UV remain editable.
    ngons=[f for f in bm.faces if len(f.verts)>4]
    if ngons:
        bmesh.ops.triangulate(bm,faces=ngons,quad_method='BEAUTY',ngon_method='BEAUTY')
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    obj.data.calc_loop_triangles()
    return {'before':before,'removed_loose_vertices':len(loose),'removed_overlapping_micro_patch_vertices':len(tiny_overlap),'filled_hair_loops':repaired,
        'after_vertices':len(obj.data.vertices),'after_triangles':len(obj.data.loop_triangles),
        'policy':'localized closed hair holes; no global remesh, UV reproject or indiscriminate decimation'}


def rig_and_bind(obj):
    rig=common.create_rig()
    # Keep the proven semantic skeleton/Idle contract but fit its joints to this
    # actual donor. This is a new skeleton, not a claim of Manny compatibility.
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='EDIT')
    for side,sign in [('l',1),('r',-1)]:
        adjustments={
            'clavicle':((sign*.025,0,1.315),(sign*.170,0,1.315)),
            'upperarm':((sign*.170,0,1.315),(sign*.430,0,1.315)),
            'lowerarm':((sign*.430,0,1.315),(sign*.668,0,1.315)),
            'hand':((sign*.668,0,1.315),(sign*.790,0,1.315)),
            'thigh':((sign*.110,0,.940),(sign*.150,.02,.505)),
            'calf':((sign*.150,.02,.505),(sign*.170,.04,.105)),
            'foot':((sign*.170,.04,.105),(sign*.180,-.065,.050)),
            'ball':((sign*.180,-.065,.050),(sign*.185,-.125,.037)),
        }
        for name,(head,tail) in adjustments.items():
            b=rig.data.edit_bones[name+'_'+side]
            b.head,b.tail=head,tail
    bpy.ops.object.mode_set(mode='OBJECT')
    groups={b.name:obj.vertex_groups.new(name=b.name) for b in rig.data.bones}
    region_counts=Counter()
    for vertex in obj.data.vertices:
        p=vertex.co
        x=abs(p.x)
        side='l' if p.x>=0 else 'r'
        if p.z>1.43:
            weights={'head':1}
            region='head'
        elif x>.165 and p.z>1.16:
            weights=common.blend_axis(x,[(.170,'clavicle_'+side),(.240,'upperarm_'+side),
                (.397,'upperarm_'+side),(.468,'lowerarm_'+side),(.635,'lowerarm_'+side),(.697,'hand_'+side)])
            # Underarm boundary must fade, not jump from the moving arm to the
            # stationary cape at a Z threshold. Back mantle stays on the torso.
            arm_factor=min(1.,max(0.,(p.z-1.16)/.075))
            # Cloth/bracer depth overlaps mantle depth: Y is not a semantic
            # separation. Beyond the shoulder, the whole arm cross-section must
            # follow the arm; only the shoulder drape uses a smooth blend.
            radial_factor=min(1.,max(0.,(x-.205)/.135))
            arm_factor=arm_factor+(1-arm_factor)*radial_factor
            shoulder_factor=min(1.,max(0.,(x-.165)/.115))
            arm_factor*=shoulder_factor
            weights={b:w*arm_factor for b,w in weights.items()}
            weights['spine_03']=1-arm_factor
            region='arm_'+side
        elif p.z<.975:
            # Coat tails/apron blend toward the pelvis as well as the leg.
            weights=common.blend_axis(p.z,[(.105,'foot_'+side),(.178,'calf_'+side),
                (.465,'calf_'+side),(.550,'thigh_'+side),(.850,'thigh_'+side),(.975,'pelvis')])
            # The continuous trouser seat crosses X=0. Keep it on the pelvis
            # with a smooth spatial fade instead of jumping between leg bones.
            seat=min(1.,max(0.,(.075-x)/.075))*min(1.,max(0.,(p.z-.680)/.110))
            weights={name:weight*(1-seat) for name,weight in weights.items()}
            weights['pelvis']=weights.get('pelvis',0)+seat
            if p.z>.640:
                # Garment tails follow the adjacent thighs from hem to waist.
                # Both leg surfaces and skirt need a continuous hip transition;
                # pelvis-fixed apron would be pierced by the bending thigh.
                follow=min(1.,max(0.,(.980-p.z)/.285))
                left=min(1.,max(0.,(p.x+.035)/.070))
                garment={'pelvis':1-follow,'thigh_l':follow*left,'thigh_r':follow*(1-left)}
                front=min(1.,max(0.,(-p.y-.045)/.045))
                back=min(1.,max(0.,(p.y-.050)/.055))
                outer=min(1.,max(0.,(x-.170)/.090))
                factor=max(front,back,outer)*min(1.,max(0.,(p.z-.640)/.050))
                weights={name:weights.get(name,0)*(1-factor)+garment.get(name,0)*factor
                    for name in set(weights)|set(garment)}
            region='leg_'+side
        elif p.z<1.075:
            weights={'pelvis':1}
            region='belt_bag'
        elif p.z>1.360:
            weights=common.blend_axis(p.z,[(1.36,'spine_03'),(1.405,'neck'),(1.445,'head')])
            region='neck'
        else:
            weights=common.blend_axis(p.z,[(1.015,'pelvis'),(1.09,'spine_01'),
                (1.20,'spine_02'),(1.290,'spine_03')])
            region='torso_cape'
        total=sum(weights.values())
        for name,weight in weights.items():
            if weight/total>1e-6:
                groups[name].add([vertex.index],weight/total,'REPLACE')
        region_counts[region]+=1
    modifier=obj.modifiers.new('Arin V3 Linear Skin','ARMATURE')
    modifier.object=rig
    modifier.use_deform_preserve_volume=False
    obj.parent=rig
    rig['arin_model']='user supplied volumetric FBX, localized repair and donor-fitted rig'
    return rig,dict(region_counts)


def procedural_surface(obj,image):
    """Keep donor UV/color identity; add material response and modest tone changes.

    Semantic masks are actual corner attributes, editable in the source blend.
    They do not project the reference picture or fabricate facial geometry.
    """
    mesh=obj.data
    pixels=list(image.pixels)
    iw,ih=image.size
    uv=mesh.uv_layers.active.data
    tone=mesh.color_attributes.new(name='ArinTone',type='FLOAT_COLOR',domain='CORNER')
    rough=mesh.color_attributes.new(name='ArinRoughness',type='FLOAT_COLOR',domain='CORNER')
    metal=mesh.color_attributes.new(name='ArinMetallic',type='FLOAT_COLOR',domain='CORNER')
    micro=mesh.color_attributes.new(name='ArinMicroSurface',type='FLOAT_COLOR',domain='CORNER')
    mesh.color_attributes.new(name='ArinFreckleMask',type='FLOAT_COLOR',domain='CORNER')
    # Adding CustomData layers can invalidate earlier RNA collection handles.
    tone=mesh.color_attributes['ArinTone']
    rough=mesh.color_attributes['ArinRoughness']
    metal=mesh.color_attributes['ArinMetallic']
    micro=mesh.color_attributes['ArinMicroSurface']
    freckles=mesh.color_attributes['ArinFreckleMask']
    counts=Counter()
    for loop in mesh.loops:
        p=mesh.vertices[loop.vertex_index].co
        u,v=uv[loop.index].uv
        ix,iy=min(iw-1,max(0,int(u*iw))),min(ih-1,max(0,int(v*ih)))
        offset=4*(iy*iw+ix)
        r,g,b=pixels[offset:offset+3]
        # Tone is restrained to avoid destroying real embroidered color detail.
        skin=r>.50 and g>.25 and r>g*1.13 and g>b*1.13
        hair=(p.z>1.605 or (p.z>1.43 and not skin) or
              (-.15<p.x<-.03 and p.y<-.075 and p.z>1.16 and r>g*1.12 and g>b*1.1))
        green=g>=r*.96 and g>b*1.10 and p.z>.65
        ivory=min(r,g,b)>.26 and r<g*1.18 and g<b*1.40
        potion=r>g*1.7 and r>b*1.6 and .9<p.z<1.12
        brooch=abs(p.x)<.024 and 1.285<p.z<1.345 and p.y<-.09
        gold=brooch and r>g*.95 and g>b*1.35
        if gold:
            kind='brass'; multiplier=(1.0,.89,.75); rv=.36; mv=.65; bump=.00002
        elif hair:
            bright=max(r,g,b)
            kind='hair'; multiplier=(.53/(1+bright),.43/(1+bright*1.25),.33/(1+bright*1.5)); rv=.68; mv=0; bump=.00012
        elif skin:
            kind='skin'; multiplier=(1.0,.97,.93); rv=.57; mv=0; bump=.000025
        elif green:
            kind='olive_cloth'; multiplier=(.78,.83,.72); rv=.87; mv=0; bump=.00015
        elif potion:
            kind='potion'; multiplier=(1.10,.80,.78); rv=.25; mv=0; bump=.00001
        elif ivory:
            kind='ivory'; multiplier=(1.0,.98,.92); rv=.83; mv=0; bump=.00015
        else:
            kind='leather_or_trousers'; multiplier=(.82,.76,.69); rv=.67; mv=0; bump=.00011
        if p.z>1.435:
            # A facial shadow in the donor atlas is not a separate hair material.
            # Smooth spatial scalp masking preserves the full face albedo and
            # prevents color/normal facets from thresholding corner pixels.
            scalp=max(min(1.,max(0.,(p.z-1.57)/.045)),
                min(1.,max(0.,(p.y+.070)/.045)),min(1.,max(0.,(abs(p.x)-.085)/.040)))
            bright=max(r,g,b)
            hair_tone=(.53/(1+bright),.43/(1+bright*1.25),.33/(1+bright*1.5))
            skin_tone=(1.,.98,.95)
            multiplier=tuple(a*(1-scalp)+h*scalp for a,h in zip(skin_tone,hair_tone))
            rv=.60*(1-scalp)+.68*scalp
            bump=.000015*(1-scalp)+.00009*scalp
            mv=0
        elif abs(p.x)>.700:
            multiplier=(1.,.98,.95); rv=.60; bump=.000015; mv=0
        else:
            # The neck's original shadow pixels still belong to skin. A color
            # threshold made adjacent corners choose cloth/leather responses,
            # producing triangular patches. Fade a spatial neck envelope; keep
            # the original albedo and exclude the braid in front of the neck.
            clamp=lambda value:min(1.,max(0.,value))
            neck=min(clamp((p.z-1.335)/.025),clamp((.090-abs(p.x))/.020),
                clamp((.035-p.y)/.020),clamp((p.y+.120)/.025))
            multiplier=tuple(a*(1-neck)+s*neck for a,s in zip(multiplier,(1.,.98,.95)))
            rv=rv*(1-neck)+.60*neck
            bump=bump*(1-neck)+.000015*neck
            mv*=1-neck
        counts[kind]+=1
        tone.data[loop.index].color=(*multiplier,1)
        rough.data[loop.index].color=(rv,rv,rv,1)
        metal.data[loop.index].color=(mv,mv,mv,1)
        micro.data[loop.index].color=(bump,bump,bump,1)
        face_zone=skin and 1.475<p.z<1.535 and abs(p.x)<.082 and p.y<-.115
        freckles.data[loop.index].color=(float(face_zone),)*3+(1,)
    material=bpy.data.materials.new('M_Arin_Adventurer_SourceShader')
    material.use_nodes=True
    tree=material.node_tree
    tree.nodes.clear()
    def n(kind,name):
        item=tree.nodes.new(kind)
        item.name=name
        return item
    output=n('ShaderNodeOutputMaterial','Output')
    bsdf=n('ShaderNodeBsdfPrincipled','Principled BSDF')
    bsdf.inputs['Specular IOR Level'].default_value=.28
    sample=n('ShaderNodeTexImage','Original Donor Color')
    sample.image=image
    attr=n('ShaderNodeVertexColor','Semantic tone')
    attr.layer_name='ArinTone'
    color=n('ShaderNodeMixRGB','Reference palette correction')
    color.blend_type='MULTIPLY'
    color.inputs[0].default_value=1
    tree.links.new(sample.outputs['Color'],color.inputs[1])
    tree.links.new(attr.outputs['Color'],color.inputs[2])
    for channel,attribute in [('Roughness','ArinRoughness'),('Metallic','ArinMetallic')]:
        item=n('ShaderNodeVertexColor',attribute)
        item.layer_name=attribute
        tree.links.new(item.outputs['Color'],bsdf.inputs[channel])
    geometry=n('ShaderNodeNewGeometry','Meter surface coordinates')
    spots=n('ShaderNodeTexVoronoi','Subtle cheek freckles')
    spots.inputs['Scale'].default_value=510
    tree.links.new(geometry.outputs['Position'],spots.inputs['Vector'])
    threshold=n('ShaderNodeMath','Small scattered freckle dots')
    threshold.operation='LESS_THAN'
    threshold.inputs[1].default_value=.12
    tree.links.new(spots.outputs['Distance'],threshold.inputs[0])
    face_mask=n('ShaderNodeVertexColor','Face-only freckle mask')
    face_mask.layer_name='ArinFreckleMask'
    masked=n('ShaderNodeMath','Freckle intensity')
    masked.operation='MULTIPLY'
    tree.links.new(threshold.outputs[0],masked.inputs[0])
    tree.links.new(face_mask.outputs['Color'],masked.inputs[1])
    freckle_color=n('ShaderNodeMixRGB','Warm subtle freckles')
    freckle_color.blend_type='MULTIPLY'
    freckle_color.inputs[2].default_value=(.80,.67,.56,1)
    tree.links.new(masked.outputs[0],freckle_color.inputs[0])
    tree.links.new(color.outputs['Color'],freckle_color.inputs[1])
    tree.links.new(freckle_color.outputs['Color'],bsdf.inputs['Base Color'])
    noise=n('ShaderNodeTexNoise','Fine material grain')
    noise.inputs['Scale'].default_value=1150
    noise.inputs['Detail'].default_value=2
    tree.links.new(geometry.outputs['Position'],noise.inputs['Vector'])
    amount=n('ShaderNodeVertexColor','Micro surface amount')
    amount.layer_name='ArinMicroSurface'
    bump=n('ShaderNodeBump','Fine surface only')
    bump.inputs['Strength'].default_value=.20
    tree.links.new(amount.outputs['Color'],bump.inputs['Distance'])
    tree.links.new(noise.outputs['Fac'],bump.inputs['Height'])
    tree.links.new(bump.outputs['Normal'],bsdf.inputs['Normal'])
    tree.links.new(bsdf.outputs['BSDF'],output.inputs['Surface'])
    mesh.materials.clear()
    mesh.materials.append(material)
    for face in mesh.polygons:
        face.material_index=0
    return material,{'BaseColor':freckle_color.outputs['Color'],'Roughness':tree.nodes['ArinRoughness'].outputs['Color'],
        'Metallic':tree.nodes['ArinMetallic'].outputs['Color'],'bsdf':bsdf},dict(counts)


def bake(obj,rig,material,sources):
    import ArinSurface
    rig.data.pose_position='REST'
    bpy.context.view_layer.update()
    texture_out=OUT/'Textures'
    texture_out.mkdir(parents=True,exist_ok=True)
    # Existing UV islands are preserved. ArinSurface's bake helper expects a key
    # from the material's prefix; do not smart-project the donor again.
    key=material.name.removeprefix('M_Arin_')
    images={channel:ArinSurface.bake_channel(obj,'Adventurer',channel,4096,{key:sources},texture_out)
        for channel in ('BaseColor','Roughness','Metallic','Normal')}
    runtime=ArinSurface.runtime_material('Adventurer',images)
    runtime.node_tree.nodes['Principled BSDF'].inputs['Specular IOR Level'].default_value=.28
    obj.data.materials.clear()
    obj.data.materials.append(runtime)
    rig.data.pose_position='POSE'
    bpy.context.view_layer.update()
    manifest={'method':'preserved donor UV + actual Cycles EMIT palette/roughness/metal and tangent micro-normal bake',
        'original_color_resolution':[2048,2048],'normal_convention':'OpenGL +Y tangent-space; Unreal flip_green_channel=True',
        'image_projection':False,'groups':{'Adventurer':{'resolution':[4096,4096],'mesh':obj.name,
        'material':runtime.name,'channels':{channel:Path(image.filepath_raw).name for channel,image in images.items()}}},
        'files':{}}
    for image in images.values():
        path=Path(image.filepath_raw)
        manifest['files'][path.name]={'bytes':path.stat().st_size,'sha256':sha(path)}
        image.filepath='//Textures/'+path.name
        image.pack()
    (OUT/'surface-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    return manifest


def presentation(obj,rig,directory):
    scene=bpy.context.scene
    scene.world=bpy.data.worlds.new('Arin V3 studio')
    camera=common.studio()
    scene.cycles.samples=24
    common.EVIDENCE=directory
    directory.mkdir(parents=True,exist_ok=True)
    for name,pos,target,scale in [
        ('idle-front',(0,-4,.95),(0,0,.87),1.95),
        ('idle-oblique',(2.6,-4,1.45),(0,0,.87),1.95),
        ('idle-back',(0,4,.95),(0,0,.87),1.95),
        ('idle-face',(.15,-2,1.60),(0,0,1.535),.38),
        ('idle-face-profile',(2,0,1.6),(0,0,1.535),.38),
    ]:
        common.render(camera,name,pos,target,scale)
    rig.data.pose_position='REST'
    bpy.context.view_layer.update()
    common.render(camera,'neutral-front',(0,-4,.95),(0,0,.87),2.40)
    rig.data.pose_position='POSE'
    bpy.context.view_layer.update()
    camera.location=(2.6,-4,1.45)
    camera.rotation_euler=(Vector((0,0,.87))-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=1.95


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--preview',action='store_true')
    parser.add_argument('--probe',action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    OUT.mkdir(parents=True,exist_ok=True)
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    obj,image,normalization=normalized_source()
    repair=repair_surface(obj)
    rig,regions=rig_and_bind(obj)
    if args.probe:
        common.idle(rig)
        rest=[v.co.copy() for v in obj.data.vertices]
        rows=[]
        for frame in (1,16,31,46,61):
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            evaluated_mesh=evaluated.to_mesh()
            ratios=[]
            for edge in obj.data.edges:
                a,b=edge.vertices
                original=(rest[a]-rest[b]).length
                ratio=(evaluated_mesh.vertices[a].co-evaluated_mesh.vertices[b].co).length/max(original,1e-8)
                if ratio>3:
                    ratios.append({'edge':edge.index,'ratio':ratio,'rest_a':list(rest[a]),'rest_b':list(rest[b])})
            evaluated.to_mesh_clear()
            rows.append({'frame':frame,'edges_over3':len(ratios),'worst':sorted(ratios,key=lambda r:-r['ratio'])[:12]})
        action=rig.animation_data.action
        rig.animation_data.action=None
        for side in ('l','r'):
            for bone in rig.pose.bones:
                bone.rotation_quaternion=(1,0,0,0)
                bone.location=(0,0,0)
            common.world_rotation(rig,'thigh_'+side,(1,0,0),-25)
            common.world_rotation(rig,'calf_'+side,(1,0,0),70)
            bpy.context.view_layer.update()
            evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            evaluated_mesh=evaluated.to_mesh()
            ratios=[]
            for edge in obj.data.edges:
                a,b=edge.vertices
                original=(rest[a]-rest[b]).length
                ratio=(evaluated_mesh.vertices[a].co-evaluated_mesh.vertices[b].co).length/max(original,1e-8)
                if ratio>4:
                    ratios.append({'edge':edge.index,'ratio':ratio,'rest_a':list(rest[a]),'rest_b':list(rest[b])})
            evaluated.to_mesh_clear()
            rows.append({'pose':'knee_'+side,'edges_over4':len(ratios),'worst':sorted(ratios,key=lambda r:-r['ratio'])[:12]})
        rig.animation_data.action=action
        (EVIDENCE/'rig-probe.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
        print(json.dumps(rows),flush=True)
        return
    material,sources,surface_regions=procedural_surface(obj,image)
    common.idle(rig)
    source_shader_file=OUT/'Arin_SourceShader.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(source_shader_file))
    if args.preview:
        presentation(obj,rig,EVIDENCE/'preview')
        bpy.ops.wm.save_as_mainfile(filepath=str(EVIDENCE/'preview/preview.blend'))
        return
    manifest=bake(obj,rig,material,sources)
    common.OUT=OUT
    common.export([obj],rig)
    presentation(obj,rig,EVIDENCE/'candidate')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Arin.blend'))
    report={'status':'exported_requires_independent_validation','input_sha256':sha(INPUT),
        'blender_version':bpy.app.version_string,'runtime_meshes':1,'materials':1,
        'triangles':repair['after_triangles'],'bones':len(rig.data.bones),
        'bone_names':[b.name for b in rig.data.bones],'head_joint_z_m':rig.data.bones['head'].head_local.z,
        'units':'meters Zup front-Y','normalization':normalization,'repair':repair,'rig_regions':regions,
        'surface_regions_corner_counts':surface_regions,'idle_seconds':2,'surface_manifest':str(OUT/'surface-manifest.json'),
        'source_scripts_sha256':SOURCE_SCRIPT_HASHES,'files':{}}
    for path in OUT.iterdir():
        if path.suffix in {'.blend','.fbx','.glb'}:
            report['files'][path.name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    obj.data.calc_loop_triangles()
    report['triangles']=len(obj.data.loop_triangles)
    (EVIDENCE/'model-build.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report),flush=True)


if __name__=='__main__':
    main()
