# 출발 전 마을 메뉴 정지와 남은 개발 계획

기준은 UE 5.8 / e5ec1703이며, 작업 브랜치는 `Sprint#9-25-safe-menu-pause`다. 기존 `Sprint#9-24`의 next-run QA 소스·실행기·11월1일 기록·미추적 증거는 보존했다. 그 작업을 검증 완료로 바꾸거나 이번 커밋에 섞지 않는다.

## 동작

정확한 `/Game/Rogue10m/Maps/StartHub` 패키지(PIE prefix 정규화), Standalone, `WaitingToStart`를 현재 안전 문맥으로 판정한다. 안전지대에 있기만 할 때는 멈추지 않는다. 인벤토리/장비/스킬/설정/영구 특성 창을 열면 월드를 정지하고, 마지막 대상 창을 닫으면 자신이 만든 정지만 해제한다. 인벤토리와 장비는 함께 열 수 있다.

GameMode는 컨트롤러별 pause delegate handle을 저장한다. 메뉴 닫기에서 전역 `ClearPause()`를 호출하지 않으므로 외부의 unbound/bound pause를 지우거나 조건을 평가하지 않는다. 메뉴 중 새 외부 Controller pause 요청도 따로 등록한다. 런 phase 변경과 EndPlay에서는 메뉴 handle만 정리한다. Tick은 추가하지 않았다.

I/P/K/O/Esc/F10은 pause 중에도 실행된다. 메뉴 진입 전에 기본 좌/우클릭·차징·brawler 버퍼와 자유 단축키의 대기 입력을 취소해, 메뉴를 닫고 마우스를 늦게 놓아도 이전 공격이 실행되지 않게 했다. 이미 수락된 공격 hit timer·몽타주·공격 행동 종료 시각·쿨타임은 이 취소 함수가 변경하지 않는다.

현재 필드에서는 안전 provider가 false이므로 메뉴가 월드·AI·런 타이머를 멈추지 않는다. 필드 안전 volume, NPC 대화 자체, 미래의 별도 상점 UI는 이번 구현에 포함하지 않는다. 맵/원본 에셋/세이브 스키마/성장·경제·보스 수치는 바꾸지 않았다.

## 실행 근거

- 최초 제품 빌드 성공 102.63초. 후속 입력 보완에서 Controller 지역변수 `Character`가 상속 멤버를 가리는 C4458을 발견해 `MenuCharacter`로만 수정했다. 실패 로그를 보존하고, 최종 실제 CPP 컴파일·링크는 47.69초 성공했다. 기존 XGE 라이선스 경고는 standalone 빌드를 방해하지 않았다.
- `VerifySafeMenuPause.ps1 -Label MenuPause1103A`: NullRHI 71/0, native fixture 15.193초, engine exit0, 기존356개 저장 SHA256 유지, 새 QA2개만 생성했다.
- `VerifySafeMenuPause.ps1 -Label MenuPause1103R -Rendered -TimeoutSeconds 180`: 창/전역 입력을 점유하지 않는 실제 D3D11 RenderOffscreen 71/0, fixture 15.119초, exit0, 실행 전358개 저장 SHA256 유지, QA2개만 추가했다.
- 두 실행에서 pause 중 닫기 키 I/P/K/O/Esc/F10 및 F6 전환, I+P 중첩, 외부 pause 선행/후행/조건 미평가, phase 전환, 늦은 mouse release의 기술 사용 횟수·자원 불변, 실제 active-run TimerManager 진행을 확인했다.
- 런처 리뷰에서 PowerShell 배열 덧셈 우선순위 문제를 수정했다. native의 정확한 namespace 거부는 처음부터 별도 유지됐으며, 수정한 런처의 Town/Fighter 기존 파일 거부 2개와 AST 검사도 통과했다. 이 3개는 UE 142검사와 별개다.
- 기존 공격 비용/스킬 조건/무기 교체/쿨타임/런 재화 회귀 결과는 아래 최종 부록에 기록한다.

사진은 `DevLog/20261103_SafeMenuPause_MenuPause1103R/`의 실제 PNG3개다. 모두1280x720 전체 디코딩·SHA256 대조와 Root 시각 검토를 마쳤다. 실제 인벤토리·중첩 장비창·활성 런 설정창이 보인다. 장비창 스탯 일부 잘림은 기존 UI 문제로 별도 남기며, 사진 한 장만으로 시간 정지를 증명하지 않는다. 시간 정지/진행 증거는 실제 world time과 TimerManager 수치 검사다. 게임 화면을 다시 그리거나 영상으로 꾸미지 않았다.

## 검증 한계와 다음 작업

안전 Hub의 타이머는 원래 WaitingToStart라 비활성이다. 이번 안전 정지 검사는 월드 시간이 멈추는 것을 확인했으며, 활성 런 타이머 진행은 같은 QA Hub에서 명시적으로 StartRun한 비안전 fixture로 검사했다. 실제 필드 왕복·정상20분·사람의 물리 입력·새 모션 품질·cook/package 완료를 뜻하지 않는다. starter Knuckle에서 `BasicBrawler` 활성은0이므로 실제 held mouse 검사는 legacy 입력 경로다. brawler의 이미 수락된 hit/몽타주 보존은 소스 검토 범위와 기존 회귀 범위를 구분한다. 특성창 실사용과 EndPlay의 별도 native 시나리오는 후속이다.

남은 개발 순서는 [계획](../architect/2026-11-03_safe-menu-pause-and-roadmap.md)에 정리했다. 우선 동일 프로필 다음 판/포기 왕복을 기존 #9-24 드라이버로 검증하고, Ctrl/Q/E/R4액티브·무기별 프리셋, Discovery 정보 숨김, 시야/보스 대체 대응·60초 직행, 필드 상점·의뢰·선택형 성장, 자원 융합, 최종 패키징으로 진행한다. Ctrl 좌우 정책·비전투 provider·경제 및 융합 수치는 임의 확정하지 않는다. 녹화 취소는 유지하고 공유 PC 전역 입력을 재점유하지 않았다.

## 최종 회귀·보존·Git 상태

기존6suite 총30개가 오류0/경고0/미실행0으로 통과했다: AttackResources4, HybridSkillTree3, ProgressionRequirements4, WeaponCooldown5, WeaponSwitch8, RunCurrencyBaseline6. 첫 suite4개 통과 뒤 바깥 셸이 오래된 LASTEXITCODE를 읽어 연쇄 실행이 중단됐지만 native 실패는 아니었다. 통과한 suite는 반복하지 않고 나머지26개만 이어서 실행했다.

작업 시작의 실제 저장356개가 모두 같은 SHA256으로 남아 있다. native QA4파일만 추가되어 현재360개이며, 원래 next-run WIP5파일의 바이트도 동일하다. 이전 SprintChangeLog 미커밋 항목은 그대로 두고 새 항목만 덧붙였다. 원본 에셋/맵 변경은 없다.

원격 fetch 결과 원격 main/develop/test는 e32e5abe, 현재 기준 e5ec1703는 main보다24커밋 앞서고 뒤진 커밋은0이었다. 이번 기능은 별도 작업 브랜치에만 기록하며, 기존 #9-24 다음 판 연속 QA가 미검증이므로 그 변경을 섞거나 develop/test/main을 자동 승격하지 않는다. 실제 커밋·push 결과는 DevLog의 최종 기록을 따른다.
## 작업 브랜치 기록 완료

제품/QA/계획/사진21파일의 기능 커밋은90d6b82874de92970b9f93a8ad65d2f72a6fc9ef이며 GitHub chickenbuger/Rogue10m의 Sprint#9-25-safe-menu-pause에 정상 push했다. ls-remote의 동일 전체 해시를 확인했다. 최초 push는 게시 승인 근거 부족으로 자동검토에서 실행 전 거절됐으나, 2026-10-27 사용자의 큰 기능 완료 후 GitHub commit/push 요청을 대화 원문에서 확인한 뒤 허용된1회 재시도로 성공했다. 추가 권한이나 재승인은 요청하지 않았다.

기존 next-run 소스·실행기·11월1일 로그·Sprint#9-24 항목은 작업폴더에 그대로 남아 있다. develop/test/main과 원본 세이브·에셋은 변경하지 않았다. 다음 판 연속 플레이 QA가 아직 미검증이므로 통합 승격은 후속으로 남긴다.