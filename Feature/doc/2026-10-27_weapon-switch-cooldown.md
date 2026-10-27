# 무기 타입 전환의 공유 공격 쿨타임 체크포인트

이번 묶음은 너클↔대검 등 무기 타입 전환의 공유 공격 쿨타임을 구현·검증했다. 전체 무기 교체 기능은 진행 중이다.

## 구현

- 실제 타입 변경은 `max(기존 남은 공유 공격 쿨타임, 0.5초)`를 적용한다. 긴 절대 만료 시각을 유지하며 0.5초를 더하지 않는다. 비장착 상태에서도 월드 시간이 흐르면 만료된다.
- 콤보가 예약한 미래 시작 쿨타임은 교체로 콤보를 취소할 때 즉시 차단 구간으로 바꾼다. 원래 만료 시각은 보존한다.
- 같은 타입 재알림은 새 하한을 부여하지 않는다. 초기 Character Data 적용과 Inventory 복원은 명시적인 C++ `RestoreEquippedWeaponType` 경로로 분리했다. 기존 Blueprint 함수 서명은 유지했다.
- 저장 형식, 숙련·랭크 정책, 런 종료 초기화, 인벤토리 내용, 에셋·맵은 변경하지 않았다.

## 검증 결과

UE 5.8.3 CL58210709, Editor Win64 Development. 재개 기준 빌드는 42.29초에 통과했고 기존 11개 테스트도 통과했다. 최종 변경의 빌드와 테스트 준비 수정 빌드까지 모두 통과했다. 마지막 빌드는 7.85초다. 중간 XGE 빌드에는 `License not activated` 경고 1개가 있었으며 기존 설치 도구를 변경하지 않았다.

변경 후 `WeaponCooldown_verified` 5개 + `AttackResources_verified` 4개 + `HybridSkillTree_verified` 3개 + `ProgressionRequirements_verified` 4개, 총 **16/16 Success, 오류 0, 테스트 경고 0**이다. `Scripts/VerifyAttackResourceCosts.ps1`에서 Suite에 WeaponCooldown을 추가했고 기존 증거를 덮어쓰지 않는다.

새 테스트는 0/0.2/0.5/5초 경계, 초기화·복원, 같은 타입 재알림, 콤보 예약, 왕복 교체와 시간 경과를 확인한다. 실제 Character 타입 설정·프로필 교체와 Combat의 실제 공격 함수도 호출해 하한 중 자원 차감/사용 횟수가 늘지 않고 만료 후 1회 공격·1회 차감되는 것을 확인했다.

검증은 NullRHI 격리 월드 자동화다. 렌더링 플레이, 실제 키 입력, 공격 명중·애니메이션·교체 모션, 사용자 프로필 전체 로드, 상점 장비 교체를 수동 검증한 것은 아니다. 테스트는 GameInstance 없이 임시 월드를 사용하며 실제 사용자 슬롯을 읽고 쓰지 않는다.

실패 이력도 보존했다. `WeaponCooldown_candidate`는 테스트 월드의 WorldContext 미등록 때문에 Tick 중 assertion/exit3이 발생했다. 월드 등록·해제를 추가했다. `WeaponCooldown_final`은 4통과/1실패였고, float 반환값과 double 월드 시간 비교의 기본 오차 범위 때문에 실패한 검사를 0.1ms 허용 오차로 명시했다. 그 후 verified에서 5개 모두 통과했다. 실패 실행을 통과로 재표기하거나 덮어쓰지 않았다.

## 보존과 근거

- 원본 `D:\Project\Rogue10m`: SHA `e32e5abe672dd9554b3f51e90b672f6039077d29`, 브랜치 `Sprint#9-1-map-maker-live-qa`, git status 깨끗함.
- 원본 SaveGames 98개는 재개 전후 SHA256 비교에서 98개 동일, 차이 0이다. 작업 Content와 원본 Content 모두 일반 물리 디렉터리이며 junction이 아니다.
- 중단 당시 미커밋 문서 6개를 상위 task의 `resume-preservation-20261027` 원문/patch에 보존하고 해시 대조했다. 로컬 보존 커밋 `415dd1e7`로 별도 기록했다. 미확인 상태였던 `STOPPED_CHECKPOINT.json`과 `stopped-uncommitted-docs.patch`는 재개 시 없었다.
- 현재 작업 브랜치 `Sprint#9-5-weapon-switch-runtime`. merge/push/PR/새 설치/YouTube 작업 없음.
- Library v1 패키지는 이번 묶음에서 다운로드·무결성 검증하지 않았다. 이전 실패를 재시도하지 않았으며 부모의 텍스트 인계와 로컬 `Docs/RapidFieldRules.md`를 기준으로 했다.

증거: `Feature/doc/evidence/weapon-switch-cooldown-20261027/`의 각 실행 `report/index.json`, automation.log, 빌드 로그와 checkpoint.json. 원본 세이브의 개별 경로/해시 목록은 task 상위 파일로 유지한다.

## 남은 구현과 다음 묶음

1. 기존 저수준 타입 setter와 Inventory 장비 변경은 아직 공격·사망 중 교체를 차단하지 않는다. 아이템 변경 전에 판정하는 단일 교체 명령과 공격 행동 시간 경계부터 연결한다. 새 쿨타임 테스트의 저수준 설정 호출은 이 정책을 검증하지 않는다.
2. 서로 다른 동일 타입 아이템 교체는 현재 타입 API가 구분하지 못하므로 아이템 인스턴스 기준 실제 교체에서 하한을 적용해야 한다. 현재 같은 타입 setter는 재알림으로 취급한다.
3. 기존 공유 공격 쿨타임을 유지했다. 스킬별 독립 쿨타임 원장이나 회피/맨손 전용 전투/아이덴티티 발동 등 별도 타이머에 하한을 적용한 것은 아니다.
4. Ctrl/Q/E/R 4슬롯 프리셋과 무기별 기억·전환, 비전투 편집, 입력/HUD, 짧은 교체 동작 및 무적 없음 검증이 남았다. E 회피와 새 E 액티브가 충돌하며 방어 키를 임의 변경하지 않았다.
5. 필드 메뉴의 계속 진행과 안전지대 메뉴의 월드/런 타이머 동시 정지, 안전지대 입장 자체는 정지하지 않는 정책이 남았다. 실제 저장 이행과 생산 맵은 계속 보류다.
