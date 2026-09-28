from pathlib import Path
import json
r=Path('D:/Project/Rogue10m');e=r/'Feature/doc/evidence/unused-file-cleanup-20260928';m=json.loads((e/'selected-cleanup.json').read_text(encoding='utf-8'))
p=r/'.gitignore';s=p.read_text(encoding='utf-8-sig');lines=[f"/{g['folder']}/frame_*.png" for g in m['frame_groups']];missing=[x for x in lines if x not in s.splitlines()]
if missing:p.write_text(s.rstrip()+'\n\n# Reproducible labeled video frames; keep source captures and generators\n'+'\n'.join(missing)+'\n',encoding='utf-8')
recovery='''# 정리 목록과 복구 안내

`selected-cleanup.json`에 삭제 대상별 경로·SHA-256·크기와 보존한 의존성이 기록되어 있다. `deleted-files.jsonl`은 실제 삭제 영수증이다. `preview.log`와 `apply.log`에서 실행 결과를 확인한다.

## 중복 파일

kind가 duplicate인 항목은 keep 경로에 동일한 바이트의 파일이 남아 있다. 원래 파일이 없는 경우에만 keep을 original로 복사하면 복구된다. 이미 원래 경로에 파일이 있으면 덮어쓰지 말고 해시와 내용을 확인한다.

## 임시 자막 프레임

kind가 regeneratable_frame인 항목은 source에 원본 스크린샷이 남아 있다. 압축된 최종 영상에서 추출하여 복원하는 방식이 아니다. 프로젝트 루트에서 아래 생성 스크립트를 실행하면 자막 PNG를 다시 만들 수 있다. 현재 원본을 이용한 전 프레임 RGB 픽셀 일치를 확인했다. PNG 메타데이터·압축 바이트의 동일성까지 보장하지는 않는다.

생성 스크립트는 프레임 외 기존 연락판/대표 JPEG를 다시 쓸 수 있다. 원본 Saved 스크린샷, required_log, 스크립트, Windows 폰트가 필요하며 이번 정리에서 보존했다. 영상 인코딩을 다시 하려면 프레임 생성 후 기존 인코더를 실행한다. 생성되는 프레임의 Git 제외 규칙은 .gitignore에 추가했다.

'''
for script in dict.fromkeys(g['generator'] for g in m['frame_groups']):recovery+='- `python "'+script+'"`\n'
recovery+='\nPython에는 Pillow가 필요하다. 실행 당시 사용한 Python은 `C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`이다. 최종 영상은 각 그룹의 final_video에 보존했다.\n'
(e/'RESTORE.md').write_text(recovery,encoding='utf-8')
print('Ignore rules and restoration guide saved.')
