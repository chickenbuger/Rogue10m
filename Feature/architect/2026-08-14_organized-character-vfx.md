# 캐릭터 파티클 정리 설계

## 목표

- 이동용과 주먹 공격용 Niagara를 콘텐츠 브라우저에서 역할별로 분리한다.
- 외부 샘플 Niagara의 원본 크기와 반복 시간이 그대로 노출되지 않도록 공격 VFX 튜닝을 Data Asset으로 이동한다.
- Cast, Charge, Impact 효과가 같은 공격 스킬을 사용하는 모든 종족에서 동일한 값으로 재생되게 한다.
- 기존 이동 먼지의 절제 기준과 AutoRelease·Timer 기반 수명 관리를 유지한다.

## Ultrawork Packets

### Packet 1 - 자산 구조 정리

- 이동: `/Game/Rogue10m/VFX/Character/Movement`
- 주먹 공격: `/Game/Rogue10m/VFX/Character/Combat/Unarmed`
- 제작 원본: `/Game/Rogue10m/VFX/Character/Source`
- 기존 평면 경로의 자산은 Unreal Editor API로 이동하고 참조를 다시 저장한다.

### Packet 2 - 공격 VFX 튜닝 데이터화

- `URogue10mAttackSkillData`에 Cast/Charge/Impact 스케일, Cast/Impact 방출 시간, Niagara 시간 배율을 추가한다.
- 단일·체인 공격은 작고 짧게, 차징 공격은 입력 중 유지하되 크기를 제한한다.
- Cast와 Impact는 Timer로 방출을 종료하고 AutoRelease 풀링을 사용한다.

### Packet 3 - 검증 및 문서화

- 새 폴더 경로, 공격 스킬 참조, 튜닝 값, 변환기 런타임 의존성을 자동 검증한다.
- Editor target 빌드, UnrealEditor-Cmd 구성/검증, Harness 검사, diff 검사를 수행한다.

## 목표 튜닝

| 분류 | 스케일 | 방출/유지 |
|---|---:|---:|
| 일반 Cast | 0.22 | 0.10초 |
| 2타 Cast | 0.24 | 0.11초 |
| 3타·특수 Cast | 0.26 | 0.12초 |
| 차징 Release | 0.32 | 0.16초 |
| Charge 유지 | 0.18 | 입력 유지 중 |
| Impact | 0.28 | 0.14초 |

모든 공격 Niagara는 기본 시간 배율 2.0을 적용한다.

## 안전 기준

- `.uasset` 이동과 저장은 Unreal Editor API로 수행한다.
- 공격 판정, 피해량, Montage, Combo Window는 변경하지 않는다.
- 이전 중복 자산은 참조자 0개 확인 후 삭제 대신 `tmp` 백업으로 이동해 복구 가능하게 한다.
