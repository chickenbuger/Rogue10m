# 설정 메뉴 포기 검증

검증 UTC: 2026-10-29T12:09:53.135740+00:00
프로젝트 D:\Project\Rogue10m, 브랜치 Sprint#9-9-abandon-menu. 커밋은 verification.json 참조.

정상 저장 95항목(D3D11 렌더링), 저장 실패 103항목(D3D11 렌더링), 기존 회귀30개 PASS. 실제 UMG OnClicked delegate와 합성 Slate Escape를 사용하는 프로그램 검증이다. 물리 마우스/키보드나 수동 한 판 완주·강제종료는 검증하지 않았다. StartHub 월드에 Running 상태를 부여한 QA 전용 문맥이므로 실제 필드 이동·밸런스 검증이 아니다.

최종 PNG6장은 실제 UE viewport의 설정, 확인 정책, 취소 후 설정, 정상 저장 결과, 귀환 경계, 허브 귀환이다. 원본 PNG 열기/디코딩(1280×720)을 확인했다. 저장 실패 화면은 DevLog/20261029_MenuAbandon_MenuLayoutF1029/04_abandon_result.png와 05_retry_boundary.png에 있다. 이 폴더의 제목 배치는 마지막 여백 보완 이전이며 최신 확인창은 MenuPolishN1029를 본다. 첫 배치 보완 전 캡처도 MenuResumeF1029에 보존한다. 귀환 직후 캡처에는 기존 shader 준비/sky mesh 진단 문구가 있을 수 있으며 최종 아트 품질 증거가 아니다.

재현: Scripts/VerifyMenuAbandon.ps1 -Label <새_고유값> -Mode Normal 또는 SaveFailure. 실제 화면은 -Rendered. 기존 슬롯과 Label은 재사용하지 않는다. QA 전용 저장만 사용하며 마을 비용 비환불, 입장 재화 복원, 런 초기화, Discovery 유지, 실패 재시도 및 허브 travel을 검증한다.

상세 결과: Feature/doc/2026-10-29_abandon-menu.md. 원시 로그: Feature/doc/evidence/menu-abandon-20261029. 이미지·실제 세이브·원시 로그는 Git 커밋 대상에서 제외한다.

봉인된 로컬 커밋: `a426d95f8f0fe6b2da3aa849680ff2fc27e9f4a5`. 추적 파일 clean, UE QA 프로세스 없음. push/병합 없음.


## 대표 사진 보관 상태

사용자 요청에 따라 중복·이전·보조 사진을 정리했다. 현재 `20261029_MenuAbandon_MenuPolishN1029` 묶음에는 대표 1장을 보관한다. 이 문서의 촬영 개수·파일명·시각 판정은 당시 실행 이력이며, 현재 사진 목록은 [대표 사진 목록](../README.md)을 따른다. 수치 결과·텍스트 실패 기록은 그대로 유지했다.
