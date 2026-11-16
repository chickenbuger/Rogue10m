# 남은 지형·전투 기능 분리 검증과 게시

### Sprint#14-1 — 저장소 파일 구성 분석·남은 기능 게시 기록

- 분석: 고정6bfc78c6 main은11,963파일·7.57GB다. 당시 로컬 변경17,271=수정74+미추적17,197이며 사용자가 본17,269는2개 차이로 과거 스냅샷 없이 특정하지 않는다. 미추적2.73GB 중 Feature/DevLog의 원본검증 자료가 대부분이다.
- 조치: PDB4개442MB만 정확경로 ignore, 물리본·해시보존. .blend1·벤더·WP·PNG·QA 저장은 삭제하지 않았다. CSV는로컬,집계JSON·보고서만게시.
- 완료 게시: 마법명령버퍼88d0bef2(6사례1701),지형참조검사6bf1c026(Editor113.28초·5/5·Error0),전리품스택계산6bfc78c6(9사례/50000무작위/2103255),프로필초기화bb7a6bd6(Editor44.35초·1/1·Error0·Warning15). 각기능 작업/develop/test/main원격SHA일치확인.
- 남음: Crop맵1453패키지+재질은 NPC클래스14개와Tutorial/Monster/Widget변경·원래Compact/HLOD2개에의존. 도로cold47/47은현재로컬DLL기준이며전체main독립폐쇄아님. Undo/Redo즉시composite·E19/E27정밀ray실패보존. 전투표현은main에없는슬롯/입력/무기·자산선행필요. 런타임전리품도main에없는기존loot/run-terminal선행필요. 미완료를합격으로바꾸지않았다.
- 관련: Feature/doc/2026-11-17_repository-file-audit.md, Feature/doc/2026-11-17_remaining-features-publication.md.

[Notion 요약 후보] 대량 파일 수는 대부분 검증 자료와 UE 에셋이었다. 독립 검증된 네 패킷을 각각 게시했고, 지형·전투 통합은 빠진 선행 기능과 남은 실패를 구분했다.
