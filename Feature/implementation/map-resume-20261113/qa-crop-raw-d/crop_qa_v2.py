"""Offline 9x5 Landscape preservation QA; never imports project/UE code.

Exit 0: offline numeric checks passed; preservation is still NOT certified.
Exit 1: explicit data/contract mismatch. Exit 2: I/O/schema/output error.
Exit 3 with --require-ready: missing raw inventory or unresolved preservation gate.
Writes only to a new output directory under this isolated packet directory.
"""
from __future__ import annotations
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

HERE = Path(__file__).resolve().parent
EV = 'Feature/doc/evidence/map-resume-20261113/'
PROJECT_INPUTS = {
    'checkpoint_b': 'Feature/doc/2026-11-13_map-integration-checkpoint-b.md',
    'checkpoint_c': 'Feature/doc/2026-11-13_map-integration-checkpoint-c.md',
    'public_review': 'Feature/implementation/map-resume-20261113/landscape-rewrite-audit/public-landscape-rewrite-review.md',
    'terrain': EV+'TerrainSnapshotReloadA/terrain.json',
    'retained': EV+'TerrainSnapshotReloadA/retained-data.json',
    'reload_comparison': EV+'TerrainSnapshotReloadA/comparison.json',
    'world_layout': 'Docs/References/WorldMap/world-layout-20261112.json',
}
ENGINE_INPUTS = {
    'public_data_access': 'Source/Runtime/Landscape/Public/LandscapeDataAccess.h',
    'public_edit': 'Source/Runtime/Landscape/Public/LandscapeEdit.h',
    'public_proxy': 'Source/Runtime/Landscape/Classes/LandscapeProxy.h',
    'public_landscape': 'Source/Runtime/Landscape/Classes/Landscape.h',
    'public_edit_layer': 'Source/Runtime/Landscape/Classes/LandscapeEditLayer.h',
    'public_brush': 'Source/Runtime/Landscape/Public/LandscapeBlueprintBrushBase.h',
    'public_config': 'Source/Runtime/Landscape/Public/LandscapeConfigHelper.h',
    'import_implementation': 'Source/Runtime/Landscape/Private/LandscapeEdit.cpp',
    'edit_implementation': 'Source/Runtime/Landscape/Private/LandscapeEditInterface.cpp',
    'layers_implementation': 'Source/Runtime/Landscape/Private/LandscapeEditLayers.cpp',
    'layer_implementation': 'Source/Runtime/Landscape/Private/LandscapeEditLayer.cpp',
    'landscape_implementation': 'Source/Runtime/Landscape/Private/Landscape.cpp',
    'slowtask_implementation': 'Source/Runtime/Core/Private/Misc/SlowTask.cpp',
    'pcg_cache': 'Plugins/PCG/Source/PCG/Public/Grid/PCGLandscapeCache.h',
}
CATEGORIES = ['editLayers', 'brushes', 'splines', 'foliage', 'pcg', 'externalReferences', 'landscapeProperties', 'componentOverrides', 'externalPackages', 'compositeNormalsAndBorderHalo']


def sha(b):
    return hashlib.sha256(b).hexdigest()


def strict_json(b):
    def unique(pairs):
        d = {}
        for k, v in pairs:
            if k in d:
                raise ValueError('Duplicate JSON key: '+k)
            d[k] = v
        return d
    return json.loads(b.decode('utf-8-sig'), object_pairs_hook=unique, parse_constant=lambda x: (_ for _ in ()).throw(ValueError('Nonfinite JSON constant '+x)))


def require(ok, text):
    if not ok:
        raise ValueError(text)


def integer(v):
    return isinstance(v, int) and not isinstance(v, bool)


def finite(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def dims(rect):
    require(len(rect) == 4 and all(integer(v) for v in rect), 'Inclusive rectangle must contain four integers')
    require(rect[2] >= rect[0] and rect[3] >= rect[1], 'Invalid inclusive rectangle')
    return rect[2]-rect[0]+1, rect[3]-rect[1]+1


def source_to_destination(x, y, policy):
    if policy['mappingMode'] == 'preserve_section_coordinates':
        return x, y
    if policy['mappingMode'] == 'rebase_sections_to_zero':
        r = policy['cropSampleRectInclusive']
        return x-r[0], y-r[1]
    raise ValueError('Unsupported mapping mode')


def world_position(x, y, origin, scale):
    return [origin[0]+scale[0]*x, origin[1]+scale[1]*y]


def mapping(policy):
    r = policy['cropSampleRectInclusive']
    full = policy['sourceSampleRectInclusive']
    w, h = dims(r)
    fw, fh = dims(full)
    q = policy['componentQuads']
    require(integer(q) and q > 0, 'Positive integer quads required')
    require(all(full[i] <= r[i] <= full[i+2] for i in (0,1)) and all(r[i+2] <= full[i+2] for i in (0,1)), 'ROI outside source')
    require(all(v % q == 0 for v in r), 'ROI boundaries must align to original component boundaries')
    require((w-1) % q == 0 and (h-1) % q == 0, 'ROI is not a whole component lattice')
    require(policy['subsectionsPerComponentAxis'] == 1, 'This codec supports the observed one-subsection case only')
    nx, ny = (w-1)//q, (h-1)//q
    require([nx,ny] == policy['retainedComponents'] == [9,5], 'Expected retained9x5 components')
    require(full == [0,0,1008,1008] and [fw,fh] == [1009,1009] and policy['sourceComponents'] == [16,16], 'Expected observed source extent0..1008/16x16')
    require(len(policy['sourceOriginCm']) == len(policy['sourceScaleCm']) == 3 and all(finite(v) for v in policy['sourceOriginCm']+policy['sourceScaleCm']) and min(policy['sourceScaleCm']) > 0, 'Invalid transform')
    origin = policy['sourceOriginCm'][:]
    scale = policy['sourceScaleCm']
    if policy['mappingMode'] == 'rebase_sections_to_zero':
        origin[:2] = world_position(r[0],r[1],origin,scale)
    dr = [*source_to_destination(r[0],r[1],policy), *source_to_destination(r[2],r[3],policy)]
    components = []
    for sy in range(r[1],r[3],q):
        for sx in range(r[0],r[2],q):
            dx, dy = source_to_destination(sx,sy,policy)
            components.append({'sourceSectionBase':[sx,sy], 'destinationSectionBase':[dx,dy], 'sourceComponentIndex':[sx//q,sy//q], 'destinationComponentIndex':[dx//q,dy//q], 'worldMinCm':world_position(sx,sy,policy['sourceOriginCm'],scale)})
    # Check every canonical retained sample; world XY must not move.
    max_error = 0.0
    for sy in range(r[1],r[3]+1):
        for sx in range(r[0],r[2]+1):
            dx,dy = source_to_destination(sx,sy,policy)
            a = world_position(sx,sy,policy['sourceOriginCm'],scale)
            b = world_position(dx,dy,origin,scale)
            max_error = max(max_error,abs(a[0]-b[0]),abs(a[1]-b[1]))
    require(max_error < 1e-7, 'World XY drift detected')
    bounds = world_position(r[0],r[1],policy['sourceOriginCm'],scale)+world_position(r[2],r[3],policy['sourceOriginCm'],scale)
    n=w*h
    return {'sourceInclusiveRect':r,'destinationInclusiveRect':dr,'destinationActorOriginCm':origin,'scaleCm':scale,'worldBoundsCm':bounds,'worldSizeM':[(bounds[2]-bounds[0])/100,(bounds[3]-bounds[1])/100], 'canonicalSamples':n,'heightRGBytes':2*n,'rawBABytesEach':n,'paintBytesPerTarget':n,'retainedComponents':len(components),'removedComponents':256-len(components),'componentStoredSamples':len(components)*(q+1)**2,'seamDuplicatesBeyondCanonical':len(components)*(q+1)**2-n,'everySampleMaximumWorldXYErrorCm':max_error,'componentMap':components,'r16SufficientForFullFidelity':False}


def canonical_plane(data, input_rect, roi, bpp):
    iw,ih=dims(input_rect)
    ow,oh=dims(roi)
    require(bpp in (1,2), 'Expected normalized u8/u16le channel')
    require(len(data) == iw*ih*bpp, 'Channel byte length/stride mismatch')
    require(input_rect[0] <= roi[0] <= roi[2] <= input_rect[2] and input_rect[1] <= roi[1] <= roi[3] <= input_rect[3], 'Channel rectangle does not cover ROI')
    result=bytearray()
    for y in range(roi[1],roi[3]+1):
        start=((y-input_rect[1])*iw+roi[0]-input_rect[0])*bpp
        result.extend(data[start:start+ow*bpp])
    require(len(result)==ow*oh*bpp, 'Canonical plane incomplete')
    return bytes(result)


def stitch_tiles(tiles, roi, q, bpp):
    """Read normalized one-subsection component tiles; reject missing/unequal seams."""
    w,h=dims(roi)
    expected={(x,y) for y in range(roi[1],roi[3],q) for x in range(roi[0],roi[2],q)}
    require(len(tiles)==len(expected) and {tuple(t['sectionBase']) for t in tiles}==expected, 'Missing/duplicate/outside component tiles')
    result=bytearray(w*h*bpp)
    seen=bytearray(w*h)
    duplicate=0
    for t in tiles:
        sx,sy=t['sectionBase']
        data=t['data']
        require(len(data)==(q+1)**2*bpp, 'Unexpected component tile byte length')
        for j in range(q+1):
            for i in range(q+1):
                index=(sy+j-roi[1])*w+sx+i-roi[0]
                sample=data[(j*(q+1)+i)*bpp:(j*(q+1)+i+1)*bpp]
                at=index*bpp
                if seen[index]:
                    duplicate+=1
                    require(result[at:at+bpp]==sample, 'Unequal seam at source (%d,%d)'%(sx+i,sy+j))
                else:
                    result[at:at+bpp]=sample
                    seen[index]=1
    require(all(seen), 'Missing canonical samples')
    return bytes(result),duplicate


def pack_raw_ba(alpha16, flags):
    require(integer(alpha16) and 0 <= alpha16 <= 65535 and alpha16 & 3 == 0, 'Alpha16 must contain no low flag bits')
    require(integer(flags) and 0 <= flags <= 3, 'Flags must fit low two A bits')
    return alpha16 >> 8, (alpha16 & 0xFC) | flags


def unpack_raw_ba(b,a):
    return ((b << 8) | a) & 0xFFFC, a & 3


def read_linked_file(desc, inventory_dir, manifest=None):
    relative=Path(desc['file'])
    require(not relative.is_absolute() and '..' not in relative.parts,'Linked file must remain under inventory directory')
    path=(inventory_dir/relative).resolve()
    require(inventory_dir.resolve() in path.parents,'Linked file resolved outside inventory directory')
    b=path.read_bytes()
    require(sha(b)==desc['sha256'],'Linked file SHA256 mismatch: '+str(relative))
    if manifest is not None:manifest.append({'group':'inventory_linked_file','absolutePath':str(path),'relativePath':str(relative),'sha256':sha(b),'bytes':len(b)})
    return b


def inventory_template(terrain, policy):
    return {'schemaVersion':2,'purpose':'raw_inventory_prerequisites_only_not_execution_authorization', 'sourceMap':policy['sourceMap'],'sourceLandscapeGuid':terrain['landscape_guid'],'cropSampleRectInclusive':policy['cropSampleRectInclusive'],'capture':{'utc':None,'allDescriptorsLoaded':None,'dirtyBefore':None,'dirtyAfter':None,'sourcePackageManifestSha256':None,'exporterSourceSha256':None,'exporterDllSha256':None,'files':{'sourcePackageManifest':None,'exporterSource':None,'exporterDll':None}},'guidPolicy':policy['layerGuidPolicy'],'sourceLayers':terrain['edit_layers'],'paintTargets':[{'name':x['name'],'layerInfoPath':x['layer_info_path'],'blendMethod':None} for x in terrain['paint_layers']], 'coverage':{k:{'status':'not_exported','items':None,'evidence':None} for k in CATEGORIES},'componentLayerPresence':None,'rawLookupFallbackUsed':None,'channels':[], 'destination':None, 'identityMappings':None, 'restorationPlan':{'publicApiOnly':None,'rawBASetterPass':None,'sourceBrushPointersPassedToImport':None,'layerGuidsRemainLiteral':None,'unknownExternalReferences':None,'renderCompositeReadbackValidated':None,'collisionReadbackValidated':None,'candidateColdReloadValidated':None}}


def inspect_inventory(inv, policy, terrain, inventory_dir, payload_inputs=None):
    failures,pending=[],[]
    require(inv['schemaVersion']==2,'Inventory schemaVersion must be2')
    if inv['sourceMap']!=policy['sourceMap'] or inv['sourceLandscapeGuid']!=terrain['landscape_guid'] or inv['cropSampleRectInclusive']!=policy['cropSampleRectInclusive']:
        failures.append('inventory.source_identity_or_ROI_mismatch')
    if inv['guidPolicy']!=policy['layerGuidPolicy']:
        failures.append('inventory.GUID_policy_changed_without_separate_review')
    capture=inv.get('capture',{})
    for key in ('utc','sourcePackageManifestSha256','exporterSourceSha256','exporterDllSha256'):
        if not capture.get(key):pending.append('capture.'+key)
    for name in ('sourcePackageManifest','exporterSource','exporterDll'):
        desc=capture.get('files',{}).get(name)
        if desc is None:pending.append('capture.linked_file.'+name)
        else:
            read_linked_file(desc,inventory_dir,payload_inputs)
            if desc['sha256']!=capture.get(name+'Sha256'):failures.append('capture.linked_SHA_mismatch:'+name)
    if capture.get('allDescriptorsLoaded') is not True:pending.append('capture.allDescriptorsLoaded')
    if capture.get('dirtyBefore')!={'maps':[],'content':[]} or capture.get('dirtyAfter')!={'maps':[],'content':[]}:pending.append('capture.clean_source_before_and_after')
    expected_layers={x['guid'] for x in terrain['edit_layers']}
    if inv.get('rawLookupFallbackUsed') is True:failures.append('inventory.raw_layer_lookup_fell_back_to_composite')
    elif inv.get('rawLookupFallbackUsed') is not False:pending.append('inventory.rawLookupFallbackUsed_must_be_false')
    presence=inv.get('componentLayerPresence')
    if presence is None:
        pending.append('inventory.component_x_raw_layer_presence')
    else:
        r=policy['cropSampleRectInclusive'];q=policy['componentQuads']
        expected_presence={(x,y,g) for y in range(r[1],r[3],q) for x in range(r[0],r[2],q) for g in expected_layers}
        actual_presence={(p['sectionBase'][0],p['sectionBase'][1],p['layerGuid']) for p in presence}
        if len(presence)!=len(expected_presence) or actual_presence!=expected_presence or any(p.get('hasLayerData') is not True or p.get('heightTexturePresent') is not True for p in presence):failures.append('inventory.missing_component_raw_layer_data_or_height_texture')
    if {x['guid'] for x in inv['sourceLayers']}!=expected_layers or len(inv['sourceLayers'])!=len(expected_layers):failures.append('inventory.layer_GUID_set_mismatch')
    metadata=['class','name','index','visible','locked','persistent_textures','editing_tools','height_alpha','weight_alpha']
    for layer in inv['sourceLayers']:
        old=next((x for x in terrain['edit_layers'] if x['guid']==layer['guid']),None)
        if old is not None and any(layer.get(k)!=old.get(k) for k in metadata):failures.append('inventory.layer_metadata_changed:'+layer['guid'])
    target_paths={x['layer_info_path'] for x in terrain['paint_layers']}
    if {x['layerInfoPath'] for x in inv['paintTargets']}!=target_paths or len(inv['paintTargets'])!=len(target_paths):failures.append('inventory.paint_target_set_mismatch')
    if any(x.get('blendMethod') is None for x in inv['paintTargets']):pending.append('paintTargets.blendMethod')
    for category in CATEGORIES:
        c=inv['coverage'].get(category,{})
        if c.get('status')=='complete' and isinstance(c.get('items'),list) and c.get('evidence'):
            if c['items']:pending.append('review_nonempty_'+category+'_migration')
        elif c.get('status') in ('unsupported','incomplete'):
            failures.append('inventory.'+category+'.'+c['status'])
        else:pending.append('inventory.'+category)
    zero='0'*32
    needed={(zero,'height_rg',None)}
    needed.update((zero,'paint',p) for p in target_paths)
    for guid in expected_layers:
        needed.update((guid,k,None) for k in ('height_rg','raw_b','raw_a'))
        needed.update((guid,'paint',p) for p in target_paths)
    actual={}
    channel_report=[]
    def payload(desc):
        return read_linked_file(desc,inventory_dir,payload_inputs)
    for c in inv['channels']:
        key=(c['layerGuid'],c['kind'],c.get('paintTargetPath'))
        if key in actual:failures.append('inventory.duplicate_channel:'+str(key));continue
        if key not in needed:failures.append('inventory.unexpected_channel:'+str(key));continue
        bpp=2 if c['kind']=='height_rg' else 1
        if c['encoding']!=('u16le' if bpp==2 else 'u8') or c['sampleOrder']!='x_fast_y_increasing':failures.append('inventory.channel_encoding:'+str(key));continue
        if 'tiles' in c:
            tiles=[{'sectionBase':t['sectionBase'],'data':payload(t)} for t in c['tiles']]
            canonical,duplicates=stitch_tiles(tiles,policy['cropSampleRectInclusive'],policy['componentQuads'],bpp)
        else:
            canonical=canonical_plane(payload(c),c['sampleRectInclusive'],policy['cropSampleRectInclusive'],bpp)
            duplicates=None
            if c.get('seamEvidence') is not None:
                evidence=strict_json(read_linked_file(c['seamEvidence'],inventory_dir,payload_inputs))
                if evidence.get('seamDuplicateCount')!=4832 or evidence.get('seamMismatchCount')!=0 or evidence.get('canonicalSha256')!=sha(canonical):failures.append('channel_seam_evidence_mismatch:'+str(key))
            else:pending.append('channel_seams_missing_linked_evidence:'+str(key))
            pending.append('flat_channel_seam_attestation_not_recomputed:'+str(key))
        actual[key]=canonical
        channel_report.append({'layerGuid':key[0],'kind':key[1],'paintTargetPath':key[2],'canonicalSha256':sha(canonical),'canonicalByteCount':len(canonical),'seamDuplicatesChecked':duplicates,'binarySourceFilesReadOnly':True})
    pending.extend('missing_channel:'+str(k) for k in sorted(needed-actual.keys(),key=str))
    ba_summary=[]
    for guid in expected_layers:
        if (guid,'raw_b',None) in actual and (guid,'raw_a',None) in actual:
            bs,aas=actual[(guid,'raw_b',None)],actual[(guid,'raw_a',None)]
            flags=Counter()
            nondefault=0
            non7f7f=0
            for b,a in zip(bs,aas):
                alpha,flag=unpack_raw_ba(b,a)
                require(pack_raw_ba(alpha,flag)==(b,a),'Raw BA codec lost bits')
                flags[str(flag)]+=1
                nondefault+=((b,a)!=(255,255))
                non7f7f+=((b,a)!=(127,127))
            ba_summary.append({'layerGuid':guid,'flagsHistogram':dict(flags),'non_ffff_count':nondefault,'non_7f7f_count':non7f7f,'note':'Engine raw-layer initialization uses0x7F7F; no authored-data equivalence inferred from statistics. Preserve every B/A bit.'})
    plan=inv.get('restorationPlan',{})
    if plan.get('sourceBrushPointersPassedToImport') is True:failures.append('plan.source_brush_pointer_reownership_risk')
    if plan.get('publicApiOnly') is False:failures.append('plan.nonpublic_API_route')
    if plan.get('layerGuidsRemainLiteral') is False and policy['layerGuidPolicy'].startswith('literal_'):failures.append('plan.Import_new_GUID_violates_literal_identity')
    if plan.get('unknownExternalReferences') is True:failures.append('plan.unmapped_external_references')
    for k in ('publicApiOnly','rawBASetterPass','layerGuidsRemainLiteral','renderCompositeReadbackValidated','collisionReadbackValidated','candidateColdReloadValidated'):
        if plan.get(k) is not True:pending.append('plan.'+k)
    if plan.get('sourceBrushPointersPassedToImport') is not False:pending.append('plan.sourceBrushPointersPassedToImport_must_be_false')
    if plan.get('unknownExternalReferences') is not False:pending.append('plan.no_unknown_external_references')
    if inv.get('destination') is None:pending.append('destination.candidate_inventory_and_per_channel_comparison')
    else:pending.append('destination.explicit_identity_mapping_and_cold_readback_independent_review_required')
    return {'explicitFailures':failures,'pendingData':sorted(set(pending)),'channels':channel_report,'rawBASummary':ba_summary,'inventoryPrerequisitesComplete':not failures and not pending,'fullFidelityPreservationCertified':False,'executionAuthorizedByChecker':False}


def run(args):
    project=args.project_root.resolve();engine=args.engine_root.resolve();out=args.outdir.resolve()
    require(HERE in out.parents and not (project==out or project in out.parents),'Output must remain under this isolated packet directory and outside project')
    require(not out.exists() or not any(out.iterdir()),'Use new/empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    manifest=[];raw={}
    for group,base,inputs in [('project',project,PROJECT_INPUTS),('engine',engine,ENGINE_INPUTS)]:
        for key,relative in inputs.items():
            path=base/relative;b=path.read_bytes();raw[key]=b
            manifest.append({'group':group,'relativePath':relative,'sha256':sha(b),'bytes':len(b),'absolutePath':str(path)})
            if group=='project':
                target=out/'inputs'/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
    pb=args.policy.read_bytes();policy=strict_json(pb)
    result={'schemaVersion':2,'evidenceKind':'offline_numeric_and_public_API_readonly_review','createdUtc':datetime.now(timezone.utc).isoformat(),'checkerSha256':sha(Path(__file__).read_bytes()),'policySha256':sha(pb),'GUIUsed':False,'UEExecuted':False,'sourceProjectMutated':False,'actualCropPerformed':False,'fullFidelityPreservationCertified':False,'checks':[]}
    def check(k,passed,detail=None):result['checks'].append({'id':k,'passed':bool(passed),'detail':detail})
    try:
        mp=mapping(policy);result['mapping']=mp
        terrain,retained,comparison=[strict_json(raw[k]) for k in ('terrain','retained','reload_comparison')]
        check('saved_source_is_still_1800m_256_components',terrain['map_package']==policy['sourceMap'] and terrain['component_count']==terrain['collision_component_count']==256 and terrain['streaming_proxy_count']==16 and terrain['edit_layer_count']==1 and terrain['retained_9_by_5_lattice_observed'] is False)
        check('saved_source_transform_matches_policy',[terrain['actor_origin_cm'][a] for a in 'xyz']==policy['sourceOriginCm'] and all(abs(terrain['actor_scale'][a]-policy['sourceScaleCm'][i])<1e-10 for i,a in enumerate('xyz')) and all(terrain['actor_rotation_pitch_yaw_roll'][a]==0 for a in 'xyz'))
        check('saved_source_extent_matches_policy',[terrain['landscape_extent']['min'][a] for a in 'xy']+[terrain['landscape_extent']['max'][a] for a in 'xy']==policy['sourceSampleRectInclusive'])
        expected={tuple(c['sourceSectionBase']) for c in mp['componentMap']}
        actual=[tuple((c['section_base'][a] for a in 'xy')) for c in terrain['components'] if c['keep_in_9_by_5']]
        check('saved_retained_component_indices_45',len(actual)==45 and set(actual)==expected)
        check('inclusive_ROI_counts_bytes_and_duplicates',mp['canonicalSamples']==retained['retained_sample_count']==179488 and mp['heightRGBytes']==retained['height_byte_count']==358976 and mp['seamDuplicatesBeyondCanonical']==4832)
        groups=[retained['composite']]+retained['raw_edit_layers']
        check('saved_RG_paint_groups_complete',all(g['complete'] and g['height']['sample_count']==179488 and g['height']['matching_seam_duplicates']==4832 and len(g['paint_weights'])==2 and all(p['sample_count']==179488 and p['matching_seam_duplicates']==4832 for p in g['paint_weights']) for g in groups))
        check('saved_cold_reload_digests_match_but_exclude_BA',comparison['same_digest'] is True and comparison['same_metadata'] is True and comparison['raw_height_BA_included'] is False and comparison['crop_performed'] is False)
        result['historicalSnapshot']={'digestAlgorithm':retained['digest_algorithm'],'semanticDigest':retained['semantic_snapshot_digest'],'metadataDigest':retained['metadata_digest'],'dataBoundary':retained['data_boundary'],'notRawPayloadArrays':True,'rawBAIncluded':False}
        result['inventoryTemplate']=inventory_template(terrain,policy)
        inv=result['inventoryTemplate'] if args.inventory is None else strict_json(args.inventory.read_bytes())
        if args.inventory:
            manifest.append({'group':'inventory','absolutePath':str(args.inventory.resolve()),'relativePath':args.inventory.name,'sha256':sha(args.inventory.read_bytes())})
        result['preservationGate']=inspect_inventory(inv,policy,terrain,args.inventory.parent.resolve() if args.inventory else HERE,manifest)
        check('inventory_no_explicit_contract_failures',not result['preservationGate']['explicitFailures'],result['preservationGate']['explicitFailures'])
        result['publicAPIGaps']=['Import requires a new/blank Landscape; it is not a crop-in-place API.','Import with InImportLayers duplicates edit-layer GUIDs and passes no raw alpha16/flags or packed BA to SetHeightData.','DuplicateLayerAndMoveBrushes can re-own brush pointers; do not pass original source brush objects.','Explicit raw BA restoration can use alpha16+flags together, or the verified packed InNormalData path with InCalcNormals=false; Import alone does neither.','GetHeightmap(validGuid) can fall back to composite when per-component LayersData is missing; validate raw layer presence before reading.','Raw BA defaults, layer GUID mapping, all brush/PCG/spline/foliage/external references, component overrides and cold readback remain prerequisites.','NullRHI IsUpToDate may return true without proving new composite/render/collision completion.']
    except (ValueError,KeyError,TypeError,IndexError,StopIteration) as exc:
        check('input_or_mapping_validation',False,type(exc).__name__+': '+str(exc))
    for e in manifest:
        try:e['sha256AtEnd']=sha(Path(e['absolutePath']).read_bytes())
        except OSError:e['sha256AtEnd']=None
        e['unchangedDuringRun']=e['sha256AtEnd']==e['sha256']
    check('captured_input_files_unchanged',all(e['unchangedDuringRun'] for e in manifest),'Per-file check only, not full project atomicity')
    failed=[c['id'] for c in result['checks'] if not c['passed']]
    result['summary']={'numericStaticPass':not failed,'failedChecks':failed,'pendingCount':len(result.get('preservationGate',{}).get('pendingData',[])),'fullFidelityPreservationCertified':False,'safeNonGUIRewritePathProven':False,'actualCropPerformed':False}
    (out/'input-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (out/'crop-policy.json').write_bytes(pb)
    (out/'checker-source.py').write_bytes(Path(__file__).read_bytes())
    (out/'inventory-template.json').write_text(json.dumps(result.pop('inventoryTemplate',{}),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print(json.dumps({'result':str(out/'result.json'),**result['summary']}))
    return 1 if failed else (3 if args.require_ready and not result.get('preservationGate',{}).get('inventoryPrerequisitesComplete') else 0)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project-root',type=Path,required=True);p.add_argument('--engine-root',type=Path,required=True);p.add_argument('--outdir',type=Path,required=True)
    p.add_argument('--policy',type=Path,default=HERE/'crop-policy.json');p.add_argument('--inventory',type=Path);p.add_argument('--require-ready',action='store_true')
    try:return run(p.parse_args())
    except (ValueError,OSError,json.JSONDecodeError) as exc:print(json.dumps({'error':str(exc)}),file=sys.stderr);return 2


if __name__=='__main__':sys.exit(main())
