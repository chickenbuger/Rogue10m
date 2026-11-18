# NPC 대화 선택 UI 개발 재개 결과

## 현재 상태
Sprint#9-29-humanoid-boss-npc의 기존 미커밋 작업을 보존하고 중단된 F 빌드를 재개했다. 1인칭 신규 개발은 보류하고 3인칭을 유지한다. 본 재개 작업에서 맵·바이너리 에셋·전투 수치·초기 무기 지급 정책은 변경하지 않았다. 커밋·푸시·통합 브랜치 승격은 하지 않았다.

## 구현과 교정
- 기존 실제 UButton 7종 서비스와 보급/닫기, Slate 키보드 선택 구현을 완성 검증한다.
- F의 C4458 세 건을 TownSlot / ScaleSlot / DialogueCharacter로 교정했다. 경고를 숨기거나 컴파일 정책을 완화하지 않았다.
- 특성 자식창을 닫을 때 자기 메뉴 pause 해제 → NPC UI 표시 복원 → visible 카드 focus 순서로 정리했다. 타인의 이동/시점 ignore 이유는 보존한다.
- 인벤토리/설정 메뉴가 NPC 대화를 인계받을 때 근접 안내문을 명시적으로 숨긴다. 메뉴 pause 때문에 NPC 타이머가 멈춰도 안내문이 남지 않는다.
- 신규 맨손 캐릭터에 장비 강화 성공을 기대하던 QA를 교정했다. 미장착 거부와 무과금 확인 후 기존 가방의 너클만 공개 grid 장착 API로 장착해 강화 성공 경로를 검사한다. 실제 신규 캐릭터의 무기 자동 장착 정책은 변경하지 않았다.
- 특성 복귀 후 첫 마우스 클릭 전에 실제 user focus와 키보드 선택/구매를 검사한다. 캡처 02/03/04는 렌더 요청 해제와 PNG 파일 완료를 기다린 뒤 다음 UI로 전환한다.
- Manual QA는 서비스 자동검사를 건너뛰므로 PASSED 대신 MANUAL_SETUP_COMPLETE로 보고한다.
- C++ -game QA에만 -DisablePython과 en/en-US를 적용해 기존 Editor AgentSkill 초기화와 언어 고정 기대값 오류를 분리했다. 사용자 언어·에디터 Python·오류 심각도는 변경하지 않았다.

## 통과한 검증
- ResumeF2 통합 Editor 빌드 38.21초, ResumeF3 19.02초, 최종 F4 10.98초 성공. F2의 XGE 라이선스 경고는 보존했고 F4 컴파일 오류는 없다. 무기 애니메이션 authoring 소스도 통합 빌드를 차단하지 않는다. 실제 대검 모션 활성화·그립·타격 렌더 품질을 완료했다는 뜻은 아니다.
- NpcResumeF3: 서비스 검사 56개 실패0, 기존 저장398개 SHA-256 보존. 최종 화면 증거는 보완 후 F4를 기준으로 한다.
- NpcResumeF4: 서비스 검사58개 실패0, 결과 파일 쓰기 확인까지 엔진 보고59개 실패0. D3D11 실제 렌더, PNG6개 개별 확인, 기존 저장400개 SHA-256 보존, engine Error/ensure/fatal0.
- 실제 StartHub 아린과 production 데이터에서 F 입력 열기, Slate hit-test 버튼, 위아래/Enter/repeat 거부, 미장착 강화 거부, 장착 후 무기/방어구 강화, 난이도3종, 보급 재수령 거부, 특성 복귀 포커스/키보드, 인벤토리/설정 인계, 다른 ignore 이유 보존, 거리 이탈/재진입, 닫기, 동일 프로세스 save detach/attach, 실제 ApplyDamage 체력0 후 접근 거부를 확인했다.
- PNG 확인: 근접 안내와 F4 대화 버튼, 보급 수령 후 상태, 실제 특성 창, 닫힌 안내 복원, 사망 체력0과 대화 제거가 각 파일의 상태에 맞는다. 3인칭 캐릭터·NPC가 보이며 텍스트/버튼이 카드 내부에 들어간다.

## 회귀검증
NPC 기존 입력·충돌, 두 프로세스 마을 서비스 저장 복원, 스킬 슬롯3+성장3+인간형2 자동검사는 아래 최종 F9에서 모두 통과했다.

## 증거
- Feature/doc/evidence/npc-dialogue-20261110/ResumeF3/{build.log,build-final-F4.log,all-saves-before.csv}
- Feature/doc/evidence/npc-dialogue-20261110/NpcResumeF4.log 및 saves-before/after.csv
- DevLog/20261110_NpcDialogue/NpcResumeF4/{results.txt,01_proximity_prompt.png,02_selectable_dialogue.png,03_service_result.png,04_trait_branch.png,05_closed_restored.png,06_death_boundary.png}
- 설계: Feature/architect/2026-11-10_npc-dialogue-selection.md

## 검증 한계
Controller InputKey 및 실제 Slate 이벤트를 프로그램으로 보낸 검증이다. 사람이 직접 누른 물리 입력, 일반 필드 전체 런, NPC 대화 상태의 프로세스 재시작 복원, 대검 최종 모션 시각 품질은 별도다. 사망 경계는 production ApplyDamage로 체력0/접근차단을 확인했으며 사망 정산 전체 흐름을 완료한 검사가 아니다. 신규 100개 스킬이나 오라 최종 품질의 완성을 주장하지 않는다.
## 통합 회귀 중 추가 발견 (F5 이후 수정 중)
- ResumeF4 단위 검사는 슬롯/카탈로그3 + 기존 성장3 + 인간형2 합계8개 모두 succeededWithWarnings0/errors0/failed0/notRun0으로 통과했다. 증거: Feature/doc/evidence/weapon-skill-loadouts-20261110/ResumeF4/summary.json.
- NpcRegF4 실패3건은 설정창에서 근접 안내문 잔류1건과 예전 F 자유재지정 필수 fixture2건이었다. F5에서 안내문은 RefreshInputMode의 동일 world/owning player/viewport 카드만 숨기는 중앙 처리로 교정했다. 정상 NPC 대화는 제외한다.
- NpcRegF5는 현재 정책의 F 거부와 Q 장착을 검사하도록 바꾼 뒤 실제 Q 실행0을 발견했다. SkillBinding은 고정 키를 Character가 처리한다고 전환했지만 Character에 Q/E/R 물리 바인딩이 없던 실제 production 누락이다. Character의 작은 press/release wrapper로 기존 Combat 권위 경로를 연결하고 modifier 계약을 보존하는 보완을 준비했다. QA는 Q/E/R 각 실제1회·자원/쿨다운, LeftShift 차단과 해제 후 정상실행, 메뉴중 입력·닫은 뒤 release-only 무실행까지 강화한다. 최종 실행 전이므로 아직 고정 키 복구를 검증 완료로 주장하지 않는다.
- TownResumeF4는 새 캐릭터 순간이동 직후 공중에서 장착을 요구한 fixture 실패였다. production 공중 제한을 바꾸지 않고 실제 grounded/CanChangeWeapon을 최대10초 기다리도록 교정했다.
- TownResumeF5는 장착·강화·장착해제/재장착을 통과했고, 기존 레벨 포인트 특성·구 무공 해금조건 가정9건이 현 로비 재화·무공 성장 계약과 달라 추가 fixture 교정 중이다. 실패 증거는 모두 보존하며 2프로세스 저장 전체 회귀 성공을 아직 주장하지 않는다.
- 대검 Ground BlendSpace 자산은 현재 경로에 없고 런타임 load warning이 남는다. 별도 weapon authoring/활성화·시각 검수의 남은 작업으로 분리한다.

## 최종 완료: F9 전체 회귀 통과 (UTC 2026-11-10 19:02)
- 최종 Editor 빌드 F9 12.57초 성공. 동일 최종 DLL에서 아래 5개 단계 모두 통과했다.
- NpcRegF9: 92검사 실패0. Q/E/R 각 같은 실제 유료 공용 기술을 정상 장착하고 정확1회 사용, 스태미나7 감소·쿨다운 약0.75초, 실제 선택 body montage 재생을 확인했다. LeftShift 차단·해제후 정상, 메뉴중 Q/E/R 차단·닫은뒤 release-only 무실행, F 거부/기존 장착 보존, F 대화 전용과 대화중Q차단도 통과했다.
- 공격 후 잠김의 실제 원인: NpcDiagF8에서 body montage/Combat/cooldown은 끝났으나 pause=true/tick=false인 FirstPersonMesh의 Uppercut만 position0에 15초 남아 CanChangeWeapon=false였다. 3인칭의 PlayCommonMontage는 비활성 FP 재생을 건너뛰고, 무기변경 판정에서도 해당 presentation만 제외했다. body/Movement/Dead/Combat 권위 제한, 타격 시점·비용·몽타주 속도는 보존했다. F9의 세 키 모두 body/Combat busy 중 무기변경 차단, 종료 후 자연 복귀, FP montage 없음이 strict assertion과 JSON/PowerShell에서 통과했다. timeout을 늘리거나 강제 Cancel하지 않았다.
- TownResumeF9: Write71 + 별도 프로세스 Read27 검사 실패0. 특성 실제 비용·상한·GAS, 성장포인트와 로비재화 분리, 내공 구매·숙련 자동해금·Q장착, 자동저장/독립 재시작/두 프로필 전환/보급 중복거부를 검사했다. 로비 예산의 정확 QA 저장 seed와 NotifyConfirmedAttack 숙련 이벤트는 명시적인 비실플레이 fixture다. 일반 필드에서 재화를 벌거나 적을 때린 전체 런의 증거가 아니다.
- NpcResumeF9: 서비스58검사 실패0(결과 쓰기 확인 포함 엔진59). 실제 StartHub 아린·Slate 선택·구매/강화/난이도/보급·특성 복귀 focus·메뉴 인계·거리/사망/동일프로세스 save roundtrip을 재확인했다. PNG6개 보존, 최종 선택 카드 화면도 직접 확인했다. 앞 F4/F6의6개 화면도 개별 검토했다.
- TPResumeF9: 782검사 실패0, 마우스13/13. 기본3인칭·카메라 벽 충돌/복원·이동169.62cm·회피251.73cm·벽 회피39.97cm와 뉴비 기본주먹/무기기능 차단을 확인했다. PNG5개 보존. F6 대응 화면의 카메라/자세도 직접 검토했다.
- ResumeF9 단위검사: 슬롯/카탈로그3 + 성장3 + 인간형2, 총8개 succeededWithWarnings0/errors0/failed0/notRun0.
- 최종 runtime/automation 로그8개 모두 Error/ensure/fatal0. 시작 시의 일반 에디터/미존재 QA key-slot 경고와 별도 대검 BlendSpace load warning은 숨기지 않았다.
- 최초 baseline 원본 저장398개 전부 SHA-256 변경0. 최종 전체 저장432개는 실패/성공 QA 신규 슬롯까지 보존한 결과다. 생성파일 검사와 관련 diff --check 통과. 기존4개 수정 바이너리는 작업 시작 전부터 있던 변경이며 이번 재개에서 asset/map을 편집하지 않았다.
- 최종 manifest: Feature/doc/evidence/npc-dialogue-20261110/ResumeF3/final-summary.json. DLL/소스 SHA-256,5단계 결과,8개 로그 hard-error counts, 전후 저장 목록 포함. 과거 F4~F8 실패/진단 자료는 원본 그대로 남겼다.

### 최종 범위와 다음 작업
이번 NPC 선택·고정 슬롯 입력·3인칭 공격 후 잠김 packet은 완료했다. 물리 키보드/마우스, 일반 필드 전체 런, 다른7 modifier와 modifier 추가 도중 차징 해제는 미검증이다. 1인칭 신규 개발/검수는 계속 보류한다. BS_GreatSword_Ground 자산 authoring/활성화와 대검 그립·모션·타격 시각 검수, 신규 개별 기술/오라 완성은 별도 남은 범위다. 커밋·푸시·브랜치 승격은 하지 않았으며 기존 사용자 변경을 보존했다.
