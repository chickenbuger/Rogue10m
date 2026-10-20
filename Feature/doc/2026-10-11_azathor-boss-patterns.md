# 아자토르 보스 전용 패턴 — 개발 결과

상태: 구현·저장·집중 PIE 검증·화면 확인·새 에디터 재로드·독립 검토 완료. 브랜치 `Sprint#5-23-azathor-boss-patterns`. 사용자의 최종 선택에 따라 스튜디오 없이 직접 개발했다. 다른 채팅에서 제거한 Studio를 재생성하지 않았다. 커밋·푸시는 하지 않았다.

## 플레이에서 바뀐 점

기존 아자토르가 기본→정면 파동→기본→심연 영역 순서로 공격한다. 특수 공격이 쿨타임이면 기본 공격으로 이어가고, 다음 사용할 수 있는 공격의 사거리를 기존 AI에 전달한다. 새 보스 프로필을 기존 아자토르 DA에 연결했으므로 기존 `BP_OW_FinalBoss`와 북부 보스 배치가 같은 데이터를 사용한다. 맵과 Blueprint는 수정하지 않았다.

| 패턴 | 경고·타격 범위 | 일반 예고 / 회복 | 일반 쿨타임 | 기본 피해 배율 |
| --- | --- | --- | --- | --- |
| 정면 파동 | 시작 방향이 고정된 길이 850cm·폭 360cm 직사각형, 주황색 | 1.10초 / 0.65초 | 6초 | 1.45 |
| 심연 영역 | 시작 대상 위치에 고정된 반경 260cm 원, 보라색. 시전 거리 1000cm | 1.40초 / 0.80초 | 8초 | 1.60 |
| HP 50% 이하 강화 | 남은 전투 동안 유지, 경고가 붉게 바뀜 | 특수 예고 ×0.75 / 회복 ×0.80 | 특수 쿨타임 ×0.75 | 특수 피해 ×1.25 |

공격 시작 시 방향·중심·범위·강화 배율을 고정한다. 예고 중 대상이 움직여도 경고가 따라가지 않고, 예고 중 체력 단계가 바뀌어도 진행 중인 공격의 시간을 줄이지 않는다. 살아 있는 선택 대상의 범위·높이·가림을 확인해 한 번만 타격한다. 타격 순간 경고를 지우며 취소·대상 상실/사망·프로필 제거·보스 사망·EndPlay에서도 타이머와 데칼을 정리한다.

게임 로직은 기존 C++ ActorComponent와 Data Asset 스키마를 확장했다. Tick을 추가하지 않았으며 타이머·체력 델리게이트·약한 대상 참조·GC 추적 재질/데칼 참조를 사용한다. 경고는 실제 Deferred Decal 머티리얼이며 디버그 Draw가 아니다. 기존 엔진 BoxMask-2D·RadialGradientExponential 함수를 재사용했다. Beholder의 소유 애니메이션 Attack03·Attack02ST를 전용 패턴에 연결했다.

## 수정 위치와 보존

- 런타임: `Source/Rogue10m/Enemy/Rogue10mMonsterAttackPatternProfile.h`, `Rogue10mMonsterAttackPatternComponent.h/.cpp`.
- 제작·검증: `Scripts/Editor/ApplyAzathorBossPatterns.py`, `InspectAzathorBossPatterns.py`, `ValidateAzathorBossPatterns.py`, `PreviewAzathorBossPatterns.py`.
- 새 에셋: `/Game/Rogue10m/World/OpenWorld/Monsters/Azathor/`의 `DA_Azathor_BossPatterns`, `M_Azathor_Telegraph`, `MI_Azathor_FrontalWarning`, `MI_Azathor_ZoneWarning`.
- 기존 에셋 변경: `DA_OW_AbyssOverlordAzathor`의 공격 프로필 참조 한 곳.

HP 10000·기본 피해 70·EXP 3000·이동 240cm/s, 기본 공격 수치와 Idle/Walk/Attack01/GetHit/Die 애니메이션을 유지했다. 다른 5종 몬스터의 기본 프로필은 opt-in false다. 콘텐츠 비교에서 기존 6,297개 중 6,296개 SHA256 동일, 기존 DA 한 개 변경, 새 에셋 네 개, 삭제 0개를 확인했다. 기존 플레이어·NPC·HUD·보행·월드와 공유 미커밋 작업을 보존했다. 이 파일들은 기존 미추적 작업을 포함하므로 전체 Git 변경을 이번 기능의 변경으로 간주하지 않는다.

튜닝은 새 `DA_Azathor_BossPatterns`에서 조정한다. 기본 공격만으로 돌아갈 경계는 기존 아자토르 DA의 프로필 참조를 `DA_OW_BasicAttack_AbyssOverlordAzathor`로 되돌리는 것이다. 기존 전체 미커밋 파일을 일괄 복원하면 안 된다.

## 실제 검증 결과

- `Scripts/BuildEditor.ps1 -NoHotReload`: Editor 빌드 성공, 68.49초. 외부 XGE 라이선스 경고 후 standalone으로 빌드됐다. 새 reflected 스키마는 에디터 재시작 후 실제 사용했다.
- 저장된 실제 보스 Blueprint와 프로필을 native 스포너로 생성해 22개 PIE 사례·480개 확인 통과, 실패 0개, 55.688초. 게임 타이밍과 피해 값을 바꾸지 않았다.
- 기본·정면·영역의 실제 플레이어 체력 감소와 단일 타격, 옆·뒤·거리·높이 회피, 실제 Visibility 가림, 고정 중심·방향, 쿨타임·재진입, 취소·프로필 상실·대상 사망/파괴·보스 사망·EndPlay를 검증했다.
- HP 50% 초과에서 일반 상태, 정확히 50%에서 즉시 강화, 회복 후 강화 유지 확인. 정면 타격 시점 1.118418→0.835781초, 해당 방어 적용 플레이어의 체력 감소 81.5→106.875. 회복 시간의 관측 오차 범위는 일반 `[0.633907, 0.690729]`초, 강화 `[0.497082, 0.545335]`초로 겹치지 않는다.
- 실제 화면에서 주황 직사각형·보라 원형·붉은 강화 경고 확인. 화면용 PIE는 카메라를 이동하고 전용 slow-motion을 사용했다. 시간 검증은 별도 정상 속도 PIE 결과다. 세 캡처 후 PIE 종료와 임시 설정 복원을 확인했다.
- 새 에디터에서 전체 보스 튜닝·재질 Shape 0/1·Blueprint 데이터 연결·다른 5종·기본 수치를 읽기 전용 재검사했다. 마지막 상태는 OpenRunMap, PIE 없음, 미저장 콘텐츠/맵 0, 백그라운드 CPU 제한 원래 값 true, t.MaxFPS 120이다.
- 독립 Reviewer가 런타임 참조·취소·타이머·기본 공격 보존·데칼 축·적용 API·실제 QA 측정 오차를 검토했다. 남은 수정 결함을 발견하지 못했다. `CheckGeneratedChanges.ps1`과 scoped `git diff --check` 통과. 기존 미커밋 바이너리 경고는 이번 변경 목록과 구분했다.

검증 도구의 초기 오류도 숨기지 않았다. 임시 인스턴스의 EditDefaultsOnly 복사와 미노출 deferred spawn API를 제거했고, 실제 저장 프로필과 PIE 소품의 임시 변경·복원으로 수정했다. 백그라운드 3FPS와 초기 로딩 간격은 상태 전환의 정확한 관측 구간 검사로 해결했다. UE 5.8 MI scalar setter가 성공 후에도 false를 반환하는 엔진 구현을 확인해 실제 값 readback으로 검사하고, 재질 컴파일 오류 배열도 확인한다.

첫 화면 검증 도중 D3D12 `Out of video memory trying to allocate a rendering resource`로 에디터가 종료됐다. 저장된 구현·이미 통과한 정상 속도 QA는 보존됐으며 새 에디터에서 다시 로드했다. 화면 미리보기는 장시간 게임 일시정지 대신 임시 slow-motion으로 바꾸고 세 화면과 정리를 완료했다. 이 복구가 장시간 GPU 안정성 문제의 원인을 해결했다는 뜻은 아니다. 초기 오류와 중단 로그를 증거 폴더에 남겼다.

## 범위와 후속 검증

이번 QA는 로드된 숲 지형의 통제된 실제 보스 사례다. 북부 폐허까지의 이동과 전체 AI 전투, 물리적 회피 입력, 장시간 GPU 안정성·밸런스, 네트워크·패키징 검증은 별도다. 대상 사망은 실제 PlayerState 사망 플래그를 사용했으며 패배 UI는 실행하지 않았다. 보스 처치→런 승리·영구 저장은 이번 범위에 포함하지 않는다.

## 증거

- [설계](../architect/2026-10-11_azathor-boss-patterns.md)
- [빌드](evidence/azathor-boss-patterns-20261011/build-editor.log), [에셋 연결](evidence/azathor-boss-patterns-20261011/application.json)
- [실제 PIE QA](evidence/azathor-boss-patterns-20261011/runtime-validation.json), [독립 검토](evidence/azathor-boss-patterns-20261011/review.md)
- [콘텐츠 비교](evidence/azathor-boss-patterns-20261011/content-comparison.json), [새 에디터 재로드](evidence/azathor-boss-patterns-20261011/inspection-fresh.json), [최종 상태](evidence/azathor-boss-patterns-20261011/inspection-final.json)
- [화면 세션과 정리](evidence/azathor-boss-patterns-20261011/visual-preview.json)
- [최종 검증 게이트](evidence/azathor-boss-patterns-20261011/exit-gate.json)
- [정면 경고](evidence/azathor-boss-patterns-20261011/visual-frontal.png), [원형 경고](evidence/azathor-boss-patterns-20261011/visual-zone.png), [강화 경고](evidence/azathor-boss-patterns-20261011/visual-enrage.png)
