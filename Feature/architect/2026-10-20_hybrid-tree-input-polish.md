# 혼합 스킬트리 표현과 실입력 보완

2026-10-20. 브랜치 `Sprint#7-2-hybrid-tree-input-polish`, 기준 main `ddc79e5a`. 사용자 요청: 미완료 항목이 있으면 진행.

## 범위와 확인된 공백

기존 53노드·성장/전투/저장 기능은 구현 및 검증됐다. 최종 visual-review에 남겨둔 아이덴티티 마름모/공명 이중 외곽선과 실제 HUD 드래그·물리 키 선택 및 발동 검증을 이번 범위로 확정한다. 무기 제작·애니메이션·전체20분/Shipping은 다른 작업이다. 수치·SaveGame 스키마·콘텐츠 원본은 바꾸지 않는다.

공유 root는 다른 감사/보스 후속 작업 중이므로 별도 sparse worktree에서 Source/Config/Scripts/문서만 수정한다. Content는 기준 root의 읽기용 junction을 사용하며 테스트 전후 Git content tree 및 작업 파일 변경을 확인한다. 바이너리 변경과 에셋 저작은 하지 않는다. 기존 프로젝트 UI 스타일을 재사용하므로 신규 Fab 반입은 없다. 빌드 출력/QA 저장은 작업 폴더 안에서 엔진과 실제 저장 API가 생성한다.

## ULW 패킷

| 패킷 | 목표·변경 영역 | 완료 조건 | 검증 | 롤백 경계 |
| --- | --- | --- | --- | --- |
| P1 Architect | 이전 설계·검토 공백 대조, 경로 격리 | 확인된 공백과 인수 조건 고정 | 독립 설계 감사 | 계획 문서 |
| P2 Implementer | native 노드 모양과 상태 표현 | 아이덴티티 마름모·공명 이중 외곽선, 기존 선택/hover/drag 유지 | Editor 빌드, 1080p/720p 실제 PNG | UI cpp/h |
| P3 Builder/QA | 실제 Slate drag/drop·key selector·키 실행 | 엔진 입력 route부터 실제 바인딩/공격 결과와 새 프로세스 저장 복원 확인 | 격리 QA 접두사 Editor-Cmd, 기존 자동화3개 | QA cpp·검증 스크립트 |
| P4 Reviewer/Doc Writer | 독립 검토·문서·공개 게시 | 검사 통과, 한국어 로그, main/remote SHA 확인 | CheckGeneratedChanges, diff/범위/최신 원격 조상 확인 | 기능 커밋 |

실입력 검증은 OnClicked/OnDrop 델리게이트 직접 호출로 대체하지 않는다. 실제 Slate hit path·pointer down/move/up, InputKeySelector 키 이벤트와 플레이어 입력 경로를 사용한다. 실제 입력에서 제품 결함이 드러나면 범위를 설명하고 최소 수정한다. 같은 실패가2회 반복되면 원인 검토 후 접근을 조정한다. 키/드래그 임시 상태와 QA 저장은 일반 사용자 슬롯을 사용하지 않는다. 입력·복원·도형 상태를 증거로 구분한다.

## 초안 미리보기 정합성

기존 외부 HTML 대표 경로도에 격투가 마나3노드가 남아 있었다. 사용자 확정 범위와 어긋나므로 이 초안의 마나 노드·간선·설명만 제거하고 내공/투지2열로 맞춘다. 전체53노드 실제게임 화면과 구분하는 대표경로 제목을 유지한다. 수정 전 HTML은 이번 evidence에, 수정 후 사본은 ui-concepts/2026-10-20에 남긴다. 외부 시각화 경로의 같은 파일도 갱신한다.

## 실입력에서 확인한 후속 원인과 보완

첫 실행 PolishRun01은 외곽 클릭·키 선택/취소/해제·실제발동이 통과했지만 drag5개가 실패했다. QA가 cachedgeometry를 가진 숨겨진 장비창 UI_SkillPreview를 골랐으므로 실제 MainHUD 소유와 Slate ancestry로 대상을 한정한다. 실패 로그와 결과 원문을 보존한다.

동시에 실제 K창 PNG와 닫힌 HUD PNG를 대조하면 스킬트리 frame이 하단 공격 Primary/E 영역을 덮는다. 제품에서 drag동안만 패널을 흐리게/HitTestInvisible로 바꿔 실제 HUD를 노출하고, Drop/Cancel/창 닫힘/컨텍스트 해제 때 저장한 opacity/visibility를 복원한다. 메뉴의 열림·일시정지·게임 입력 차단 계약은 유지한다. 임의로 UI geometry를 옮기거나 QA에서 숨김 처리해 통과하지 않는다.

QA도 실제 사용자 입력처럼 drag threshold 다음 프레임에 HUD hit path를 다시 찾고, 놓기/취소 후 다음 프레임에 원상 복원과 다음 입력을 확인한다. 잠긴 노드는 drag 자체가 시작되지 않아야 하므로 HUD가 가려진 상태의 hit 성공을 요구하지 않고 원본 위에 release하여 바인딩 불변을 확인한다.

## 통합 준비의 Architect 재검토

최신 main에는 보스 최종 명중 Save v3 보완과 보호된 파일 감사가 들어왔다. 스킬트리 변경 경로와 겹치지 않으므로 최종 빌드 전에 기준을 갱신한다. append-only DevLog의 내 추가분을 임시 분리하고 fast-forward하려 했지만 두 번 DevLog 보존 검사에서 거절되어 참조는 바뀌지 않았다. 소스·기록 손실은 없다. 같은 명령을 반복하지 않고 독립 검토와 인덱스 상태 원인 확인으로 돌아왔다. 다음 시도는 정확한 기존 바이트와 index metadata 일치를 먼저 입증하고 수행하며, 실패하면 검증된 소유 변경의 체크포인트를 보존한 정상 merge 경로를 검토한다. 공유 root checkout/index는 사용하지 않는다.

## 콘텐츠 연결 사고와 복원

공유 원본을 보존하려고 만든 Content junction은 잘못된 격리 방식이었다. 최신 main fast-forward의 sparse checkout 정리가 junction을 따라 원본의 추적 에셋6,423개를 삭제했다. 사용자가 요청한 콘텐츠 변경이 아니며 제 작업 폴더 구성 오류다. 즉시 사용자에게 알리고 원인과 복원 범위를 기록했다.

전체 Content restore는 미커밋 에셋을 덮을 위험으로 자동 승인 검토에서 거절됐다. 읽기 감사로 실제 없는 추적 경로6,423개와 다른 Content 변경0, 기준 Content tree561dc863470c130a9b5fe511c6f9c68dd8f74b41 일치를 확인했다. 이 경로만 literal NUL manifest로 한정하고 모두 여전히 없는지 다시 확인한 뒤 restore했다. 존재하는 파일은 덮어쓰지 않았다. 복원 파일의 존재와 전체 Content diff·독립 blob 검증 결과는 별도 JSON에 기록한다.

이후 Content junction은 사용하지 않고 작업 폴더에 실제 독립 checkout을 둔다. 복원·검증이 끝나기 전 완료 판정이나 게시를 하지 않는다. base-integration.json의 공유 root 불변이라는 사전 가정은 실제 사고에 맞게 정정했다.
