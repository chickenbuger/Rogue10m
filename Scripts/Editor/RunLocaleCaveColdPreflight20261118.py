"""Fresh read-only metadata proof for the culture-corrected cold helper."""
from pathlib import Path
import importlib.util
ROOT=Path('D:/Project/Rogue10m')
s=importlib.util.spec_from_file_location('cave_cold_locale_metadata_preflight',ROOT/'Scripts/Editor/PreflightCaveColdMetadata20261118.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
r=m.main('cold-locale-metadata-preflight.json')
assert r['passed'],r.get('traceback')
print('CAVES FINAL_COLD_SAME_METADATA_IMPLEMENTATION_PREFLIGHT_PASS')
