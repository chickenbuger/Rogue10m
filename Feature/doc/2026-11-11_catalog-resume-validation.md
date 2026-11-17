# 카탈로그 재개와 실행 전 검사 구현

공유 원본은 읽기 전용으로 확인했고 새 물리 사본 tmp/CatalogResume20261111에서만 구현·검증했다. 공유 에디터·빌드 프로세스를 시작하거나 변경하지 않았다. C++ 소비 계약·게임 에셋·3인칭 카메라는 변경하지 않았고 1인칭은 계속 보류했다. 커밋·푸시는 없다.

## 현재 적용·회귀 상태

이전 소스 패치는 공유 forward-check1/reverse-check0으로 이미 반영돼 있다. Combat의 차이는 소비 계약과 별개인 후속 3P 실제 body mesh 선택이며 되돌리지 않는다. CatalogSharedA의 실제 보고서4개를 대조해 카탈로그/소비/슬롯 정책3개와 기존성장3개 모두 Success/경고0/오류0임을 확인했다. 새로 엔진 검사를 실행한 결과가 아니라 보존된 후속 통합 증거다.

이전 CatalogElevC의 이관 실패는 Content 없는 격리 사본에서 F.Ultimate의 실제 ChargedShockwave를 LoadSynchronous하지 못한 결과다. 외부 HTTP 타임아웃 경고도 당시 strict 조건 실패에 포함됐다. 이후 통합 clean 성공이 있어 현재 코드 회귀 실패로 계속 보고하지 않는다. 실패 증거 자체는 삭제하지 않았다.

## 구현한 후속

VerifyCatalogConsumption.ps1에 PreflightOnly와 실행 전 선행검사를 구현했다. 기본3개와 IncludeGrowth의6개 계획, 프로젝트/Editor/DLL 존재, 성장 검사의 실제 패키지 요구, process_created=false를 preflight.json에 기록한다. 성장 에셋 누락과 DLL 부재는 프로세스 실행 전에 차단한다. PreflightOnly는 DLL 빌드 필요 여부를 보고하되 엔진을 시작하지 않는다. 파일 존재를 실제 package load나 카탈로그 활성화 성공으로 기록하지 않는다. 기존 launcher의 content_assets=false 고정값도 요청/검사/존재 여부로 구분했다.

Scripts/Tests/test_catalog_preflight.py의 실제 PowerShell 호출8개가 12.012초에 모두 통과했다. core 무콘텐츠 사전검사, 성장 에셋 누락 차단, 파일 존재와 opt-in 구분, DLL/Editor/프로젝트 누락 차단, 기존 증거 label 보존을 검증했다. 리뷰에서 발견한 프로세스 생성 직후 메타데이터 기록 실패의 정리 누락도 수정하고 mock Start/Stop으로 실패를 주입해 finally 실행을 확인했다. 테스트의 파일 fixture는 실제 Unreal 자산/실행파일이 아니며 어떤 케이스도 실제 엔진을 생성하지 않았다.

실제 격리 사본의 ResumeCore11A는 종료0/3개 계획/build_required=true, ResumeGrowth11A는 예상 종료1과 실제 누락 패키지를 기록했다. 둘 다 process_created=false다. 이 결과를 UE 자동검사7개 성공으로 표현하지 않는다.

## 실제20개 콘텐츠 공백

canonical SkillProductionTracker의 K/G20개를 기술명+무기 계열로 C++ legacy 정의와 대조했다. 기존 소스 파일14개는 존재하지만 새 모션·고유판정·VFX 승인 근거는 아니다. K10과 G04/G05/G06/G08/G10의 legacy 대응 정의6개가 없고 K01은 기본공격과 독립된 활성 DA 분리가 필요하다. 따라서 새 데이터 선행 작업은 최소7개다. 기존 대응14개도 새 정책/모션/판정을 별도 검증해야 한다.

ResumeSword1/catalog-preintegration-readback.json은 2026-11-10 관측에서 너클·대검 active_catalog_count=0을 기록한다. 당시 프로필 해시는 기록되지 않아 오늘의 내부값을 직접 확인했다고 쓰지 않는다. 현재 프로필 해시는 별도 감사자료에 보존했다. 실제 opt-in은 에디터 소유 담당자와 조율 후 수행해야 한다.

FantasyMotionVFXSources의 예전 G행 번호는 canonical 번호와 달랐다. 격리 문서에서8행의 번호를 기술명 기준으로 바로잡고 G10행 전체가 canonical 기술명/번호와 일치하는지 검증했다. 원본 모션/에셋 내용은 바꾸지 않았다. 생성한 catalog-current-state.json도 canonical 문서 ID와 기술명/실제 DA 경로를 사용한다. 마법사 Staff와 뉴비는 근접20개 표에 넣지 않는다.

## 통합 순서와 남은 검증

이전 소비 패치를 다시 적용하지 않는다. 새 runner·테스트2개 파일을 담당자가 적용하고 PreflightOnly로 환경을 확인한다. 공유 엔진 사용 시간을 조율한 후 필요한 경우 새 label로 core 또는 IncludeGrowth를 실행한다. 새 C++ 변경이 없으므로 이번 스크립트 검증을 위해 Unreal 빌드를 시작할 필요는 없다.

실제20개 에셋 제작, 새 성장 노드/선택UI, 기존 저장의 목록 밖 장착 참조 이관, 실제3P 모션·판정·VFX·물리입력·풀런은 후속이다. 공유 파일은 임의로 반영하지 않았다. 최종 패치/해시/적용점검과 보존 증거 경로는 Delivery/README.md에서 확인한다.

## 보스 AI·카탈로그 QA 통합과 현재 DLL 재검증 (UTC 2026-11-11)
- 이전 gameplay 패치가 이미 반영된 것을 확인해 중복 적용하지 않았다. 보스 readiness 3스크립트와 기능 문서, 카탈로그 실행기/격리 검사·문서만 공유 프로젝트에 통합했다.
- 보스 엔진 없는49/49 통과. BossResume1111 실제 타깃수명5/5, strict parser·실제 종료코드0·정확 queue1회 통과, 자연 AI112 assertions 실패0·기존445저장 보존이다. 자연AI 실행의 새 QA 저장2개는 격리 증거로 남았다.
- 자연AI 파동/장판 캡처2장을 직접 확인했다. 파동은 보이나 장판 효과는 사진에서 뚜렷하지 않으므로 시각 최종 완료로 승인하지 않는다. 기술 회귀와 구분한다.
- 카탈로그 엔진 없는8/8, CatalogQa11B의 실제4단계·6/6검사 성공. 원본447개 저장 변경0·예상 밖 신규0·DLL해시 동일·실제 테스트/종료 오류0. 카탈로그는 사전/테스트·종료 red가4단계 모두0이었다. 전리품의 사전 self-test15와 구분했다.
- 카탈로그20기술 콘텐츠/선택UI/production opt-in·저장이관 완료를 뜻하지 않는다. 이 작업은 실제 구현 소비경로와 정책/성장의 현재 회귀다. 1인칭 보류·커밋/푸시 없음.
- Notion 요약 후보: 이미 반영된 보스/카탈로그를 덮어쓰지 않고 QA 도구를 통합했다. 타깃5·자연AI112·카탈로그6의 현재DLL검사를 통과했으며 콘텐츠 제작과 최종 시각 검수는 별도 관리한다.
- 근거: tmp/work-boss-ai-target-loss-20261110/Engine_BossResume1111, Feature/doc/evidence/humanoid-boss-npc-20261110/BossResume1111, Feature/doc/evidence/catalog-consumption-20261110/CatalogQa11B, tmp/catalog-current-dll-20261111/verification.json.