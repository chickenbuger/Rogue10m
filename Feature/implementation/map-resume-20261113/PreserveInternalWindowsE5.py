"""Preserve only a proven E5 internal archive in one fresh workspace tmp folder; no execution."""
import hashlib,json,shutil,types,datetime
from pathlib import Path
R=Path(__file__).resolve().parents[3]
A=Path('C:/Users/PC/AppData/Local/Temp/R10InternalSmoke20261113D')
P=R/'Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e'
raw=(P/'strict_primitives.py').read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF'
c=types.ModuleType('delivery_primitives');c.__file__=str(P/'strict_primitives.py');exec(compile(raw,c.__file__,'exec'),c.__dict__)
completed=c.load(c.stable_bytes(A/'execution-completed05.json'));assert completed['status']=='internal_artifact_and_iostore_identity_UNVERIFIED' and completed['release_eligible'] is False
post=c.load(c.stable_bytes(A/'run05/evidence/postflight.json'));assert post['status']=='internal_smoke_artifact_built_UNVERIFIED' and post['release_eligible'] is False
verify=c.load(c.stable_bytes(A/'run05/evidence/iostore-verification.json'));assert verify['status']=='iostore_csv_identity_only_UNVERIFIED' and verify['release_eligible'] is False
source=c.physical(A/'run05/archive/Windows',True);payload=c.payload_manifest(source);assert c.digest(c.canonical(payload))==post['payload_sha256']
top=c.physical(R/'tmp/InternalWindows20261113E5_UNVERIFIED');assert not top.exists() and c.child(top,R/'tmp')
assert shutil.disk_usage(R).free>sum(f['size'] for f in payload)+2*1024**3
top.mkdir();target=top/'Windows';shutil.copytree(source,target,copy_function=shutil.copy2,symlinks=False)
assert c.payload_manifest(source)==payload and c.payload_manifest(target)==payload
for name in ('postflight.json','iostore-verification.json','iostore-listing-process.json'):
 src=A/'run05/evidence'/name;assert not (top/name).exists();shutil.copyfile(src,top/name)
readme='이 파일은 미검수 내부 Windows Development 빌드입니다.\n\n현재 맵은 1800m로 축소 전이며 NPC 신규 배치와 기본맵 전환을 하지 않았습니다.\n정규 Game 재빌드/Cook/IoStore/archive 및 요구 맵 목록은 검증했습니다.\n게임 실행·UI7·DX12·물리 입력·저장 라우팅·15분 성장 완주는 미검증입니다. release_eligible=false.\n원본과 다른 폴더의 새 사본이며 자동으로 실행하지 않았습니다.\n원본 SaveGames는 빌드 전후 해시가 일치했습니다. 실제 실행 시 저장 라우팅 보존은 아직 확인하지 않았습니다.\n'
(top/'README.ko.txt').write_text(readme,encoding='utf-8')
v={'status':'UNVERIFIED_INTERNAL_DEVELOPMENT_PACKAGE_PRESERVED','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_archive':str(source),'archive':str(target),'files':len(payload),'payload_bytes':sum(f['size'] for f in payload),'payload_sha256':post['payload_sha256'],'payload':payload,'release_eligible':False,'GUI_UI_physical_play_save_routing':'UNVERIFIED','game_execution':0,'gui_operations':0,'source_postflight':c.file_pin(A/'run05/evidence/postflight.json'),'source_iostore_verification':c.file_pin(A/'run05/evidence/iostore-verification.json')}
with (top/'DELIVERY.json').open('x',encoding='utf-8',newline='\n') as f:json.dump(v,f,indent=2)
ref={k:v for k,v in v.items() if k!='payload'};ref['delivery_manifest']=c.file_pin(top/'DELIVERY.json')
with (R/'Feature/doc/evidence/map-resume-20261113/InternalSmokeD/persistent-delivery05.json').open('x',encoding='utf-8',newline='\n') as f:json.dump(ref,f,indent=2)
print(json.dumps(ref,indent=2))
