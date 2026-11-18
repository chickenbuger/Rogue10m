"""Retain the explicit asynchronous art capture module until its final tick."""
from pathlib import Path
import importlib.util
import sys
key='rogue_organic_cave_art_capture_20261118'
assert key not in sys.modules
s=importlib.util.spec_from_file_location(key,Path('D:/Project/Rogue10m/Scripts/Editor/CaptureOrganicCaveArt20261118.py'))
m=importlib.util.module_from_spec(s);sys.modules[key]=m;s.loader.exec_module(m)
