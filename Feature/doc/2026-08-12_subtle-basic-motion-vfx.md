# 기본 이동 파티클 절제 조정 결과

## 결과

걷기·뛰기·점프·2단 점프·착지·구르기의 기존 강한 이동 파티클을 짧고 둥근 회백색 먼지 Poof 계열로 교체했다. 걷기는 가장 작고 드물게, 뛰기와 공중·회피 동작은 강도에 따라 조금씩 커지도록 구성했다. 공격용 Niagara와 공통 공격 Montage는 변경하지 않았다.

## 최종 튜닝

| 동작 | 발생 간격 | 스케일 | 방출 시간 | 의도 |
|---|---:|---:|---:|---|
| 걷기 | 0.68초 | 0.12배 | 0.08초 | 좌우 발에 번갈아 작은 먼지 한 덩어리 |
| 뛰기 | 0.38초 | 0.16배 | 0.10초 | 걷기보다 읽히되 지면 가까이에 제한 |
| 점프 | 이벤트 1회 | 0.18배 | 0.12초 | 이륙 순간만 짧게 강조 |
| 2단 점프 | 이벤트 1회 | 0.20배 | 0.14초 | 공중에서 구분되는 최소 강조 |
| 착지 | 이벤트 1회 | 0.20배 | 0.14초 | 충격 지점을 짧게 표시 |
| 구르기 | 이벤트 1회 | 0.16배 | 0.12초 | 긴 잔상 없이 시작 지점만 표시 |

모든 이동 효과에는 Niagara Custom Time Dilation 3.0을 적용하고, 동작별 Timer가 방출을 조기에 끝내도록 했다. 반복형 원본이어도 화면에 쌓이지 않으며 `AutoRelease` 풀링을 유지한다.

## 자산과 런타임 구성

- StarterContent `P_Smoke`를 프로젝트 내부 원본으로 복제했다.
- UE 5.8의 공식 Cascade→Niagara 변환기로 `/Game/Rogue10m/VFX/Character/Source/NS_MotionDustPoof_Source`를 제작했다.
- 이동용 Niagara 6종은 변환된 소프트 먼지 원본을 공유한다.
- 변환 결과에는 에디터 전용 `CascadeToNiagaraConverter` 패키지 의존성이 남지 않는다.
- `URogue10mCharacterMotionDataAsset`에서 간격·스케일·방출 시간·시간 배율을 조정할 수 있다.
- `URogue10mCharacterAnimationComponent`가 이벤트와 Timer로 출력하며 Tick을 사용하지 않는다.

## 직접 PIE 테스트

2026-08-13에 Human Male 캐릭터로 실제 PIE를 실행했다. 몬스터와 스포너를 제거한 깨끗한 상태에서 걷기, 점프, 구르기 입력을 직접 실행하고 이동 VFX 누적 여부를 확인했다.

| 항목 | 결과 | 확인 내용 |
|---|---|---|
| 걷기 | 통과 | 기존의 밝은 청색 파편 반복과 화면 점유가 재현되지 않았고 이동 중 파티클이 누적되지 않음 |
| 점프 | 통과 | 점프 입력과 공통 점프 상태가 정상 실행되고 이동 VFX가 화면을 가리지 않음 |
| 구르기 | 통과 | 구르기 입력과 공통 Montage가 정상 실행되고 긴 이동 잔상이 남지 않음 |
| 단일 먼지 Poof 프레임 캡처 | 제한적 | 1인칭 HUD와 짧은 입력 자동화 타이밍 때문에 0.08~0.14초 방출의 개별 프레임을 정지 화면으로 명확히 분리하지 못함 |
| 뛰기·2단 점프 | 자동 검증 통과 | 지속 Sprint 입력과 StoneFist 프로필의 별도 시각 캡처는 이번 보정 QA 범위에서 수행하지 않음 |

개별 Poof 정지 화면의 캡처 한계와 별개로, 런타임 입력 중 기존 이동 파티클 과다 노출과 누적은 재현되지 않았다. 자동 검증기는 걷기·뛰기·점프·2단 점프·착지·구르기 6종의 Niagara 참조, 스케일, 간격, 방출 시간과 1단/2단 점프 프로필을 검사한다.

## 검증

- UE 5.8 `Rogue10mEditor Win64 Development` 빌드 성공
- 구성 스크립트 실행: `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
- 공통 검증기 통과: `locomotion=walk/run/jump/fall/land`, `niagara=12`, `subtle_motion_vfx=6`
- 변환기 런타임 패키지 의존성: 0개
- 실제 Unreal Editor PIE에서 걷기·점프·구르기 입력 및 화면 누적 여부 확인
- `Scripts/CheckGeneratedChanges.ps1`, `git diff --check` 수행

## 관련 문서

- `Feature/architect/2026-08-12_subtle-basic-motion-vfx.md`
- `Feature/doc/2026-08-12_common-character-animation-vfx.md`
- `Docs/SprintChangeLog.md`
- `DevLog/20260813.txt`
