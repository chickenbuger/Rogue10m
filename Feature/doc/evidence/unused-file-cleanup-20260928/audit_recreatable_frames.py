from pathlib import Path
from PIL import Image
import contextlib, io, json, hashlib, runpy, traceback, time

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/unused-file-cleanup-20260928/recreatable-frames.json'
CASES=[
 ('tmp/boxing-direct/labeled','tmp/boxing-direct/make-preview.py','Saved/Screenshots/WindowsEditor/BoxingDirectPreview','tmp/boxing-direct/preview.log','Feature/doc/images/boxing-direct-head-20260922/boxing-direct-head.mp4'),
 ('tmp/boxing-direct/labeled-lookdown','tmp/boxing-direct/make-lookdown.py','Saved/Screenshots/WindowsEditor/BoxingDirectPreviewLookDown','tmp/boxing-direct/lookdown.log',None),
 ('tmp/boxing-direct/labeled-lookdown40','tmp/boxing-direct/make-lookdown40.py','Saved/Screenshots/WindowsEditor/BoxingDirectPreviewLookDown40','tmp/boxing-direct/lookdown40.log','Feature/doc/images/boxing-direct-head-20260922/boxing-direct-lookdown.mp4'),
 ('tmp/boxing-reference/labeled-boxing','tmp/boxing-reference/make-gameplay.py','Saved/Screenshots/WindowsEditor/BoxingStances/boxing','tmp/boxing-reference/boxing.log','Feature/doc/images/boxing-reference-20260922/boxing-gameplay.mp4'),
 ('tmp/boxing-reference/labeled-longguard','tmp/boxing-reference/make-gameplay.py','Saved/Screenshots/WindowsEditor/BoxingStances/longguard','tmp/boxing-reference/longguard.log','Feature/doc/images/boxing-reference-20260922/longguard-gameplay.mp4'),
 ('tmp/boxing-reference/labeled-rooted','tmp/boxing-reference/make-gameplay.py','Saved/Screenshots/WindowsEditor/BoxingStances/rooted','tmp/boxing-reference/rooted.log','Feature/doc/images/boxing-reference-20260922/rooted-gameplay.mp4'),
 ('tmp/fullbody/labeled','tmp/fullbody/make-preview.py','Saved/Screenshots/WindowsEditor/MarteloFullBodyPreview','tmp/fullbody/preview.log','Feature/doc/images/full-body-first-person-20260922/full-body-first-person.mp4'),
 ('tmp/head-camera/labeled','tmp/head-camera/make-preview.py','Saved/Screenshots/WindowsEditor/HeadCameraPreview','tmp/head-camera/preview.log','Feature/doc/images/head-attached-camera-20260922/head-attached-camera.mp4'),
 ('tmp/head-framing/labeled-forward30','tmp/head-framing/make-forward30.py','Saved/Screenshots/WindowsEditor/BoxingDirectPreviewForward30','tmp/head-framing/forward30.log','Feature/doc/images/head-framing-20260922/head-view-30.mp4'),
 ('tmp/martelo-game/labeled','tmp/martelo-game/make-preview.py','Saved/Screenshots/WindowsEditor/MarteloPreview','tmp/martelo-game/runtime.log','Feature/doc/images/martelo-first-person-20260922/martelo-first-person.mp4'),
 ('tmp/relaxed-idle/labeled','tmp/relaxed-idle/make-preview.py','Saved/Screenshots/WindowsEditor/BrawlerIdleTransitionLookDown','tmp/relaxed-idle/preview.log','Feature/doc/images/relaxed-idle-20260922/relaxed-idle-combat.mp4'),
]
def sha(path):
 return hashlib.sha256(Path(path).read_bytes()).hexdigest()
rows={}
for folder,script,source,log,video in CASES:
 d=ROOT/folder; files=sorted(d.glob('frame_*.png')); all_files=[p for p in d.rglob('*') if p.is_file()]
 rows[str(d.resolve())]={
 'folder':str(d),'frame_count':len(files),'bytes':sum(p.stat().st_size for p in files),'total_folder_bytes':sum(p.stat().st_size for p in all_files),
 'non_frame_files':[str(p) for p in all_files if p not in files],
 'script':str(ROOT/script),'script_sha256':sha(ROOT/script),'source_directory':str(ROOT/source),'source_directory_exists':(ROOT/source).is_dir(),
 'required_log':str(ROOT/log),'required_log_exists':(ROOT/log).is_file(),
 'missing_source_frames':[p.name for p in files if not (ROOT/source/p.name).is_file()],
 'final_video':str(ROOT/video) if video else None,'final_video_exists':bool(video and (ROOT/video).is_file()),
 'final_video_bytes':(ROOT/video).stat().st_size if video and (ROOT/video).is_file() else None,
 'recreated_frames':[],'mismatched_frames':[],'errors':[]}

original_save=Image.Image.save; original_mkdir=Path.mkdir; original_write_text=Path.write_text

def inspect_save(im,fp,*args,**kwargs):
 if not isinstance(fp,(str,Path)): raise RuntimeError('Unexpected non-path save from source script')
 path=Path(fp); key=str(path.parent.resolve())
 if key not in rows or path.suffix.lower()!='.png': return
 row=rows[key]
 if not path.exists(): row['errors'].append('generated_frame_does_not_exist:'+path.name); return
 with Image.open(path) as actual:
  rgb_actual=actual.convert('RGB'); rgb_new=im.convert('RGB')
  actual_hash=hashlib.sha256(rgb_actual.tobytes()).hexdigest(); generated_hash=hashlib.sha256(rgb_new.tobytes()).hexdigest()
  same=rgb_actual.size==rgb_new.size and actual_hash==generated_hash
  row['recreated_frames'].append({'name':path.name,'rgb_sha256':actual_hash,'recreated_rgb_sha256':generated_hash,'exact_pixels_equal':same})
  if not same: row['mismatched_frames'].append(path.name)

started=time.time()
for script in dict.fromkeys(c[1] for c in CASES):
 script_rows=[r for r in rows.values() if r['script']==str(ROOT/script)]
 # These inspected scripts only write via these three functions. Suppress all output artifacts.
 Image.Image.save=inspect_save; Path.mkdir=lambda *a,**k:None; Path.write_text=lambda *a,**k:0
 try:
  with contextlib.redirect_stdout(io.StringIO()): runpy.run_path(str(ROOT/script),run_name='__main__')
 except Exception as e:
  for row in script_rows: row['errors'].append(type(e).__name__+': '+str(e))
 finally:
  Image.Image.save=original_save; Path.mkdir=original_mkdir; Path.write_text=original_write_text
 print(script,[(r['frame_count'],len(r['recreated_frames']),len(r['mismatched_frames']),r['errors']) for r in script_rows],flush=True)

for row in rows.values():
 row['pixel_equality_count']=sum(f['exact_pixels_equal'] for f in row['recreated_frames'])
 row['eligible_for_root_cleanup']=bool(row['frame_count'] and row['pixel_equality_count']==row['frame_count'] and not row['missing_source_frames'] and not row['errors'] and row['final_video_exists'] and not row['non_frame_files'])
 row['recommendation']='candidate_only_root_must_revalidate_before_deletion' if row['eligible_for_root_cleanup'] else 'preserve'
 if not row['final_video_exists']: row['errors'].append('No specifically matched final video located; preserve even if pixel reconstruction passes')
report={'date':'2026-09-28','operation':'read_only_audit_no_deletion','proof':'Existing labeling script executed with every save/mkdir/write_text suppressed; each newly composed full RGB frame was SHA256-compared to the existing labeled PNG pixels. No source frames or output artifacts were changed.',
 'limits':['Pixel equality, not compressed PNG byte equality.','Current source screenshots, script, required log and Windows fonts must be retained.','Final video existence checked; video was not decoded and compared frame-by-frame.','Audit is a point-in-time read. Root must revalidate folder/file hashes or existence before authorized cleanup.'],
 'font_dependencies':[{'path':p,'exists':Path(p).exists(),'sha256':sha(p) if Path(p).exists() else None} for p in ['C:/Windows/Fonts/malgun.ttf','C:/Windows/Fonts/arial.ttf']],
 'elapsed_seconds':round(time.time()-started,2),'folders':list(rows.values())}
report['candidate_bytes']=sum(r['bytes'] for r in rows.values() if r['eligible_for_root_cleanup'])
report['candidate_folders']=sum(r['eligible_for_root_cleanup'] for r in rows.values())
OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'report':str(OUT),'candidate_folders':report['candidate_folders'],'candidate_bytes':report['candidate_bytes'],'seconds':report['elapsed_seconds']},ensure_ascii=False))
