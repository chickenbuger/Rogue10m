# 기본 이동 파티클 절제 조정 설계

## 목표

- 걷기 파티클이 캐릭터 실루엣과 지면을 가리지 않도록 크기와 발생 빈도를 낮춘다.
- 참조 영상의 짧고 둥근 먼지 덩어리처럼, 기본 동작 VFX를 부드러운 단발형 스타일로 통일한다.
- 걷기, 뛰기, 점프, 2단 점프, 착지, 구르기가 같은 시각 언어를 사용하되 동작 강도에 따라 크기만 단계적으로 달라지게 한다.
- UE 5.8 Niagara, AutoRelease 풀링, 이벤트/Timer 기반 실행을 유지하고 상시 Tick을 추가하지 않는다.

## Ultrawork Packets

### Packet 1 - 소프트 먼지 Niagara 교체

- 목표: 기존 스파크 및 점프패드 기반 이동 VFX를 StarterContent 먼지 텍스처를 사용하는 Niagara 계열로 교체한다.
- 수정 위치: `/Game/Rogue10m/VFX/Character/NS_Motion_*`, `Scripts/Editor/ConfigureCommonCharacterAnimation.py`
- 완료 조건: 공식 Cascade→Niagara 변환기로 만든 소프트 먼지 원본을 이동용 Niagara 6종이 공유하고 공격 VFX는 변경하지 않는다.
- 검증 명령: `UnrealEditor-Cmd` 구성 스크립트 및 검증 스크립트
- 롤백 경계: 이동용 Niagara 6종과 구성 스크립트의 이동 VFX 매핑

### Packet 2 - 발생 밀도 및 스케일 데이터화

- 목표: 걷기/뛰기 간격, 최소 이동 속도, 동작별 스케일, 좌우 발 위치를 Motion Data Asset에서 조정할 수 있게 한다.
- 수정 위치: `Rogue10mCharacterMotionDataAsset`, `Rogue10mCharacterAnimationComponent`
- 완료 조건: 걷기는 가장 작고 드물게, 뛰기와 공중/회피 동작은 단계적으로 크게 출력되며 첫 입력 순간의 즉시 버스트가 제거된다.
- 검증 명령: `Scripts/BuildEditor.ps1`
- 롤백 경계: Motion Data 신규 파라미터와 Niagara 스폰 인자

### Packet 3 - 검증 및 시각 QA

- 목표: 구성 값, Niagara 참조, 컴파일, 실제 PIE 화면을 확인한다.
- 수정 위치: `Scripts/Editor/ValidateCommonCharacterAnimation.py`, 결과 문서, DevLog, SprintChangeLog
- 완료 조건: 빌드와 자동 검증이 통과하고 PIE에서 걷기 파티클이 발밑에 짧게 표시되며 화면을 가리지 않는다.
- 검증 명령: `Scripts/BuildEditor.ps1`, `UnrealEditor-Cmd`, `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`
- 롤백 경계: 검증기 및 문서 변경

## 목표 튜닝

| 동작 | 발생/스케일 방향 | 의도 |
|---|---:|---|
| 걷기 | 0.68초, 0.12배, 0.08초 방출 | 발 한쪽에 작은 먼지 한 덩어리 |
| 뛰기 | 0.38초, 0.16배, 0.10초 방출 | 걷기보다 선명하지만 지면에 제한 |
| 점프 | 0.18배, 0.12초 방출 | 이륙 순간만 짧게 표시 |
| 2단 점프 | 0.20배, 0.14초 방출 | 공중에서 읽히는 최소 강조 |
| 착지 | 0.20배, 0.14초 방출 | 충격 위치를 짧게 확인 |
| 구르기 | 0.16배, 0.12초 방출 | 긴 잔상 대신 시작 지점의 짧은 먼지 |

## 안전 기준

- 이동 VFX는 `UNiagaraFunctionLibrary::SpawnSystemAtLocation`과 `ENCPoolMethod::AutoRelease`를 유지한다.
- 반복형 원본 여부와 무관하게 방출 종료 Timer와 3배 Niagara 시간 배율로 1초 이내 감쇠를 보장한다.
- 효과 스케일은 Data Asset에 노출하며 런타임에서 최소값을 보정한다.
- 발자국 Timer는 이동 입력이 끊기거나 공중 상태가 되면 정리한다.
- 공격 Niagara, 공격 Data Asset, 종족별 리타기팅 자산은 수정하지 않는다.
