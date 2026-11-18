"""Reuse the physical checkpoint contract at a fresh, exclusive dated output."""
import importlib.util
from pathlib import Path
import sys

sys.dont_write_bytecode = True
root = Path('D:/Project/Rogue10m')
spec = importlib.util.spec_from_file_location('landscape_checkpoint_20261117', root/'Scripts/CheckpointLandscape20261116.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.BACKUP = root/'tmp/LandscapeResume20261117/before'
module.MANIFEST = root/'Feature/doc/evidence/landscape-resume-20261117/baseline-files.json'
raise SystemExit(module.main())
