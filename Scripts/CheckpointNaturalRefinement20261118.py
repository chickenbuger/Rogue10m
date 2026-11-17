"""Physically preserve the numeric checkpoint and seven owned assets."""
import importlib.util
import json
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('refinement_checkpoint_base',ROOT/'Scripts/CheckpointLandscape20261116.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.BACKUP=ROOT/'tmp/LandscapeNaturalRefinement20261118/before'
m.MANIFEST=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/refinement-baseline-files.json'
original_tree=m.tree_files
excluded={'Rogue10mNaturalBoundary20261117Authoring.h','Rogue10mNaturalBoundary20261117Authoring.cpp'}
m.tree_files=lambda path,required:[p for p in original_tree(path,required) if p.name not in excluded]
if __name__=='__main__':
    code=m.main()
    if code:raise SystemExit(code)
    own=ROOT/'Content/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117'
    rows=[]
    for path in m.tree_files(own,True):
        before=m.pin(path);target=m.BACKUP/before['path']
        target.parent.mkdir(parents=True,exist_ok=True)
        with path.open('rb') as src,target.open('xb') as dst:shutil.copyfileobj(src,dst)
        # Compare bytes rather than m.pin: this copy has a different path.
        import hashlib
        if hashlib.sha256(target.read_bytes()).hexdigest().upper()!=before['sha256']:raise RuntimeError('Owned copy mismatch')
        rows.append(before)
    if len(rows)!=7:raise RuntimeError('Expected seven owned assets before village-ground material')
    with (m.MANIFEST.parent/'refinement-owned-assets.json').open('x') as f:json.dump({'backed_up':rows},f,indent=2)
    print('Preserved seven owned assets in addition to candidate checkpoint')
