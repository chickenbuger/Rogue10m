"""Fresh physical checkpoint; preserve the earlier checkpoint and original maps."""
import importlib.util
from pathlib import Path
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("natural_checkpoint_base",ROOT/"Scripts/CheckpointLandscape20261116.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.BACKUP=ROOT/"tmp/LandscapeNaturalBoundary20261117/before"
module.MANIFEST=ROOT/"Feature/doc/evidence/landscape-natural-boundary20261117/baseline-files.json"
original_tree=module.tree_files
NEW_HELPERS={"Rogue10mNaturalBoundary20261117Authoring.h","Rogue10mNaturalBoundary20261117Authoring.cpp"}

def old_inputs(directory,required):
    # These two new authoring sources belong to this operation and are still
    # being implemented; they are recorded separately after the editor build.
    return [p for p in original_tree(directory,required) if p.name not in NEW_HELPERS]

module.tree_files=old_inputs
if __name__=="__main__":raise SystemExit(module.main())
