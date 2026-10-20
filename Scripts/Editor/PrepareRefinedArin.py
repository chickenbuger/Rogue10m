"""Stage and validate only 24 owned v2 dependencies; keep guide appearance."""
import importlib
import sys
from pathlib import Path
import unreal

project=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(project/'Scripts/Editor'))
import ApplyRefinedArin as application
application=importlib.reload(application)

data,blueprint,cdo,actor=application._guide_context()
before=application._guide_state(data,blueprint,cdo,actor)
if application.CHECKPOINT.exists():
    import json
    baseline=json.loads(application.CHECKPOINT.read_text(encoding='utf-8-sig'))
    application._verify_preserved(baseline,before)
else:
    application._json(application.CHECKPOINT,before)
manifest,sources,hashes=application._surface_sources()
state={'mode':'prepare_only','source_hashes':hashes}
try:
    mesh,animation,skeleton,textures,parent,materials,saved,reused=application._load_or_import(sources,hashes,False,state)
    report={'status':'prepared_only_guide_unchanged','source_hashes':hashes,
        'core':application._validate_core(mesh,animation,skeleton),
        'textures':application._validate_textures(textures,sources,hashes),
        'materials':application._validate_materials(materials,parent,textures),
        'slots':application._slots(mesh,materials),'lods':application._lod_metrics(mesh),
        'saved_assets':saved,'reused_verified_scope':reused}
    if before!=application._guide_state(data,blueprint,cdo,actor):
        raise RuntimeError('Guide changed during dependency-only preparation')
    application._json(application.EVIDENCE/'unreal-refined-prepared.json',report)
    unreal.log('ARIN_REFINED_DEPENDENCIES_PREPARED')
except Exception as exc:
    application._stage(state,'aborted_requires_inspection',error=str(exc),observed_assets=sorted(application._scope_assets()))
    raise
