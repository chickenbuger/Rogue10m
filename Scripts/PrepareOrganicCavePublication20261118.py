"""Create an explicit publication manifest after every feature gate passes.

No Git mutations. Unrelated untracked work and raw host logs are not selected.
"""
from pathlib import Path
from datetime import datetime,timezone
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Feature/doc/evidence/cave-organic20261118'
def read(p):return json.loads((ROOT/p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    manifest=BASE/'publication-manifest.json';assert not manifest.exists()
    runtime=read('Feature/doc/evidence/cave-organic20261118/runtime-camera-verified/result.json')
    assert runtime['passed']
    assert read('Feature/doc/evidence/cave-organic20261118/final-editor-state.json')['passed']
    assert read('Feature/doc/evidence/cave-organic20261118/scope-final.json')['passed']
    assert read('Feature/doc/evidence/cave-organic20261118/cold-host-direct/acceptance.json')['passed']
    assert read('Feature/doc/evidence/cave-organic20261118/qa-final-live/result.json')['passed']
    assert read('Feature/doc/evidence/cave-organic20261118/qa-final-live/render-geometry.json')['passed']
    purposes={}
    def add(p,why):
        p=(ROOT/p).resolve()if not isinstance(p,Path)else p.resolve()
        assert p.is_relative_to(ROOT)and p.is_file()
        purposes[p.relative_to(ROOT).as_posix()]=why
    add('.gitattributes','고정 소스·검증 파일의 Git 바이트 보존')
    for p in (ROOT/'Art/World/CavesOrganic20261118/V2').iterdir():
        if p.suffix in {'.obj','.json'}or p.name=='.gitignore':add(p,'원본 저작 암반 형상·법선·슬롯과 소스 OBJ 추적 예외')
    for path in ['Feature/architect/2026-11-18_organic-cave-refinement.md','Feature/doc/2026-11-18_organic-cave-refinement.md',
                 'Docs/WorldMapProductionGuide.md','Docs/References/WorldMap/README.md','Docs/References/WorldMap/cave-organic20261118.json',
                 'Docs/SprintChangeLog.md','DevLog/20261118.txt']:
        add(path,'동굴 미술·동선 보존 기획, 실제 검증 결과와 개발 기록')
    for folder in ['Scripts','Scripts/Editor']:
        for p in (ROOT/folder).glob('*Organic*20261118*.py'):add(p,'명시 호출형 저작·읽기 전용 검사·진단·재현 도구')
    add('Scripts/StartOrganicCaveCold20261118.ps1','소유한 숨김 Cmd 프로세스의 저장본 재로드·실제 종료 검증')
    save=read('Feature/doc/evidence/cave-organic20261118/author-live/final-save-result.json')
    assert save['success']and len(save['requested_packages'])==8
    for package in save['requested_packages']:
        assert package.startswith('/Game/')
        add('Content/'+package[6:]+'.uasset','Unreal Editor에서 한정 저장한 동굴 액터4개 또는 신규 재질2·암반 메시2')
    for p in BASE.rglob('*'):
        if p.is_file()and (p.suffix in {'.json','.png'}or p.name=='generated-check.txt')and p.name not in {'publication-manifest.json','publication-verification.json','final-publication.json'}:
            add(p,'기준·실패·수정·실제 화면·카메라·cold·보존 검증 증거')
    rows=[{'path':path,'purpose_ko':purposes[path],'bytes':(ROOT/path).stat().st_size,'workspace_sha256':sha(ROOT/path)}for path in sorted(purposes)]
    r={'schema':'Rogue10m.OrganicCavePublicationManifest.20261118.v1','prepared_utc':datetime.now(timezone.utc).isoformat(),
       'repository':'https://github.com/chickenbuger/Rogue10m','branch':'Sprint#9-32-landscape-world-blockout',
       'promote_integration_branches':False,'map_production_gate':'전체 맵 제작 중: 검증한 동굴 체크포인트만 작업 브랜치 게시',
       'self_hash_excluded_to_avoid_recursive_manifest':True,'raw_host_logs_selected':False,'files':rows}
    with manifest.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,ensure_ascii=False,indent=2)
    paths=[row['path']for row in rows]+[manifest.relative_to(ROOT).as_posix()]
    out=ROOT/'tmp/OrganicCaves20261118/publication.paths';assert not out.exists()
    out.write_bytes(b'\0'.join(p.encode('utf-8')for p in paths)+b'\0')
    print(json.dumps({'files':len(paths),'manifest':str(manifest),'pathspec':str(out),'bytes':sum(r['bytes']for r in rows)},ensure_ascii=False))
if __name__=='__main__':main()
