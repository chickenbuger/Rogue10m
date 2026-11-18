# DevLog 대표 사진 정리 결과

DevLog 사진 **1,618장 → 35장**으로 정리했다. 보조 사진 **1,583장·2,037,221,100바이트(약2.04GB)**를 실제 삭제했다. 대표 사진은 [DevLog 목록](../../DevLog/README.md)에서 확인할 수 있다.

## 남긴 기준

- 옵션·대화·특성·승리 정산 등 기능을 보여 주는 최종 게임 화면.
- 보스 세 패턴과 대검·너클 기본 공격, 지팡이 두 동작의 최소 대표 장면.
- 초기/반복 회차, 빈 화면, 흔들린 장면, 자동화 결과 텍스트의 사진은 제외했다. 수치 증거는 기존 JSON·텍스트로 남겼다.
- 남긴 35장은 원본 바이트·파일명·경로를 유지했다. action 사진을 접촉 판정으로 바꾸거나 이전 실패를 성공으로 재분류하지 않았다.

## 검증

- 1,583개 삭제 대상의 절대 경로가 DevLog 안에 있는지, 파일인지, image 확장자인지, reparse 경로가 아닌지와 SHA256을 모두 확인했다. 폴더 재귀 삭제는 사용하지 않았다.
- 남은 사진은 정확히 선별한35장·46,091,175바이트이며 원본 SHA256이 모두 같다.
- 기존 비사진 파일 누락0, 삭제 대상 잔존0, 삭제된 사진을 가리키는 활성 Markdown 링크0이다.
- 57개 링크를 삭제 사실이 드러나는 문구로 정리하고 55개 Markdown 문서에 현재 보관 상태를 표시했다. 과거 수치·촬영 개수·실패 기록은 이력으로 유지한다.
- 영상·기존 JSON·CSV·세이브·원문 로그는 삭제하거나 덮어쓰지 않았다. Unreal 실행·빌드·에셋 변경은 수행하지 않았다.

## 기록과 게시 범위

정리 manifest와 검증은 [사진 목록](evidence/devlog-photo-curation-20261117/photo-manifest.csv), [삭제 결과](evidence/devlog-photo-curation-20261117/deletion-result.json), [최종 검사](evidence/devlog-photo-curation-20261117/verification.json), [문서 정리](evidence/devlog-photo-curation-20261117/reference-updates.json)에 있다. 디렉터리 이름은 작업을 시작한 시점의 날짜를 유지했다.

현재 `Sprint#9-32-landscape-world-blockout` 작업 브랜치에만 commit·push한다. 맵 제작 중인 다른 코드·문서 변경과 main/develop/test는 포함하지 않는다. 이미 추적된 삭제 사진은 Git 과거 커밋에 남고 미추적 보조 사진은 실제 삭제했으며, Git 이력을 재작성하지 않았다.
