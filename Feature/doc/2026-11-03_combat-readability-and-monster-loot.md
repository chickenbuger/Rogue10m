# 전투 가독성·판정 표시·몬스터 전리품 결과

UE5.8.4(CL58902052) 업데이트 뒤 보스 회피, 1인칭 공격 예고, 중앙 크로스헤어, 테스트 전용 실제 판정 표시와 사망 전리품을 구현·검증했다. 같은 프로필의 다음 판/포기/귀환은 이전 b54b3273 체크포인트에서 완료한 기반을 유지한다.

## 구현

- 보스 기본/파동/원형 예고를 .308/1.1/1.4초에서 **1.0/1.6/1.9초** 후보로 늘리고 회복 시간을 비례 조정했다. 기본 공격의 기존 분노 예외를 보존하여 분노 시에도1.0초이며 파동1.2초·원형1.425초다. HP·공격력·범위·쿨다운·분노 배율·애니메이션 참조는 유지했다. 원본 프로필 대신 새 파생 프로필만 연결했다.
- 기본 공격 경고 material을 추가했다. 근접 정면 시야에서는 바닥 경고가 가려져, 기존 체력바가 실제 확인한 보스가 자신을 겨냥해 Windup 중일 때 패턴명·남은 시간·진행도를 표시한다. 새 조준 대상이 우선하며 null look 동안만 같은 Windup 대상을 유지한다. 메뉴·검사·사망에서 숨기고 해결 시 즉시 없앤다.
- 일반 초기 pitch -20→-10도, weighted0도와 초기화 이후 자유 시점은 유지했다. NativePaint 중앙 크로스헤어는 DPI 좌표를 따르며 검사·메뉴·사망에서 숨긴다.
- F6 판정 표시 기본 OFF. 현재 physics scene의 query 활성 Sphere/Box/Capsule, 실제 공격 overlap과 실제 양수 피해 수락 Actor를 구분한다. Arc의 실제 sphere와 Actor 각도 필터도 구분한다. 잠긴 MultiHit의 후속 pulse에는 새 overlap이 없다. 미지원 wrapper/convex/mesh는 미표시 수로 보고한다. 별도 authored collision 참고는 현재 query와 다른 전체 월드 표시다. OFF/Clear/대상 소멸/종료/소유 pawn 변경에서 전용 표시와 viewport flag를 정리한다.
- 몬스터 사망 1회 guard와 조정 가능한 전리품 표를 추가하고 기존 월드 픽업·원자적 inventory 획득에 연결했다. authority+Standalone에서 동작하며 시험 몬스터·종료 런은 제외한다. **실제 월드 VerdantSlime만 체력 물약25%×1 임시 예시**를 연결했다. 다른 몬스터는 표를 지정할 수 있으며 현재 미지정 동작을 유지한다.

## 검증

| 검증 | 결과 |
|---|---|
| 최종 Win64 Editor Development 빌드 | UE5.8.4에서77.82초 성공 |
| native 회귀 |40개 실제 행 모두Success/errors0. 전리품5개 포함.37개 일반 성공+새 QA 프로필이 아직 없다는 경고3개 성공 |
| BossRenderD |551검사/0실패, 일반·분노×3패턴×정지/걷기/Alt 18종. 정지6개 피격, 지연.25초 이동·.32초 Alt를 사용한12개 회피에서 피해0. 실제 HUD18종 및 메뉴·검사 숨김1회 |
| SpatialRenderC |85검사/0실패. 실제38도형 중 skeletal37, 미지원0·미확인0. 고정 대상 재사용·실제 피해·기본OFF·정지 상태 OFF/Clear·viewport 원상복구 확인 |
| 카메라 |업데이트 전 Normal/Weighted×720/1080 네 번과 최종 빌드 Normal720 한 번, 각49검사/0실패.25PNG의 픽셀 검출 문제0 |
| LootRuntimeC |실제 사망·collision 자동 획득·debounce/SaveNow·디스크 역직렬화·재부착26검사 및 별도 프로세스 복원9검사/0실패. 수량1·동일GUID 유지 |
| 보존 |작업 전364저장 SHA 모두 동일. 현재386개, 추가22개는 격리 QA 슬롯만. 원본 보스 프로필과 unrelated NormalSlime/물약/가방 에셋 보존 |

실제 PNG54장을 디코딩·해시 검사했다. 대표12장을 DevLog/20261103_CombatReadability_Review에 원본 그대로 발췌했고 Root가 실제 화면으로 준비·해결·회복, 판정/해제, 시야를 확인했다. 픽셀 검출은 보조 수단이며 전체 시각 품질 보증은 아니다. 모든 수치·입력은 명시된 자동 fixture이며 물리 마우스·키보드 실플레이나 이번 수정 후 전체20분 런/패키징을 수행한 것은 아니다.

## 실패 기록과 한계

첫 native 시험의 ASC 등록과 GameMode 누락을 실제 공개 월드 초기화 경로로 수정했다. 작성 A는 struct 행 편집 제한, B는 Python bool+outText 반환 오해로 저장 전에 실패했다. 독립 유효/무효 메모리 표에서 성공은 빈Text, 실패는None임을 확인한 뒤 C가 정확히 두 에셋을 저장했고 새 프로세스로 읽었다. 원본 작성 스크립트와 A/B 기록은 보존했다. 엔진의 succeeded는 warning 성공을 별도 집계하여, 전체40개 실제 결과 행을 기준으로 판정했다.

판정 A의 작은 시험 피해는 실제 자연 회복에 가려졌고 B는 비동기 포즈가 정리되지 않은 상태를 비교했다. 최종 시험은 소유한 F6 보스의 회복만 끄고 기존 포즈를 공개 API로 동기화했다. HP를 직접 쓰거나 제품 충돌·허용 오차를 바꾸지 않았다. 보스 회피 fixture는 저장에서 분리한 HP50000 플레이어와 임시 바닥을 사용한다.

바닥 경고는 capsule을 확장한 좁은 판정 참고다. 실제 피해에는 원래 raw overlap·선택 대상·높이·시야 조건도 필요하며 파동은 한 번에 해결되는 사각형이다. 녹색 점은 Actor 원점이며 접촉점이 아니다. 실제 회피 손맛·완전한 애니메이션 접촉 품질·모든 지형·멀티플레이·cook/package는 추가 확인 대상이다. 미획득 월드 전리품은 저장하지 않고 기존 v5 중단 런 포기 정책을 따른다. 강제 SpawnActor 실패는 시험하지 않았다. 픽업 외형은 native cube prototype이다.

5.8.4의 -game 로그에는 실험 Toolset Python 초기화 오류가 있으나 이번 native/게임 검증의 결과 marker와 저장 검증은 통과했다. 엔진 또는 MCP 전체가 정상이라고 주장하지 않으며 엔진 파일·플러그인은 수정하지 않았다.

## 지상형 보스 후보

2026-11-03 15:47UTC 공개 Fab 재확인. 라이브러리는 로그아웃 상태라 현재 계정 소유는 미확인이다. 취득·다운로드·장바구니·라이선스 수락을 수행하지 않았다. 현재 최종 보스의 Beholder 외형은 유지했다.

| 후보 | 확인한 범위 |
|---|---|
| 설치된 Cactus/ThornWarden·TurtleShell/IronShell |메시와 공격·이동·피격·사망 파일 존재. 현재 계정 소유 확인과 최종 보스 크기·공격 리타이밍은 별도 |
| [SciFi Beast06 Bull](https://www.fab.com/listings/126120a3-996e-4dc3-94f1-6550f0adac03) |개인/기업 무료 표시, Standard, UE5.0–5.8 package, 공격6개 포함29애니메이션. 실제 임포트 미검증 |
| [Stan — The Golem](https://www.fab.com/listings/0a80c04e-ad02-4bf3-a059-f98ee9bf782e) |무료·CC BY4.0, FBX/Blender/GLB 등. UE package가 없어 임포트 검증과 출처·변경 표시 필요 |
| [Armored Stone Golem](https://www.fab.com/listings/e40713f0-ebea-4f4d-ad9b-b577db4432d5) |개인₩88,770/기업₩355,160(VAT포함), Standard, UE5.1–5.8·25애니메이션. 무료 기간이 끝나 취득하지 않음 |

## 재현과 다음 단계

고유한 새ASCII 라벨을 사용한다. 기존 저장·근거는 지우지 않는다. Scripts/VerifyCombatReadability.ps1 -Mode Boss/Automation, VerifySpatialDebug.ps1 -Rendered -RequireLiveSkeletal, VerifyCameraCrosshair.ps1, VerifyMonsterLoot.ps1로 격리 검증을 실행할 수 있다. 프로필·progression·키맵 세 QA 옵션을 동시에 지정하는 guard가 있다. 전체 변경 파일은 final/changed-files.md, 실제 결과·좌표·해시는 final/verification.json 및 manifest에 기록했다.

남은 우선순위는 별도 remaining-development-roadmap 문서의 4액티브 프리셋/HUD → Discovery 표시 → 실제 초반 성장·자원 튜닝 → 지상형 보스 후보 적용 검증 → cook/package·전체20분 QA 순서다. 큰 디자인 규칙은 임의로 확정하지 않는다.
