# 캐릭터 파티클 정리 결과

## 결과

애니메이션에 연결된 Niagara를 이동, 주먹 전투, 제작 원본의 세 영역으로 정리했다. 기존 이동 먼지 6종은 절제된 설정을 유지하며, 주먹 공격 6종에는 Data Asset 기반 크기·방출 시간·시간 배율을 추가해 외부 샘플 원본이 과도하게 출력되지 않도록 했다.

## 자산 구조

| 경로 | 자산 | 역할 |
|---|---:|---|
| `/Game/Rogue10m/VFX/Character/Movement` | 6종 | 걷기·뛰기·점프·2단 점프·착지·구르기 |
| `/Game/Rogue10m/VFX/Character/Combat/Unarmed` | 6종 | 주먹 1~3타·차징·차징 방출·적중 |
| `/Game/Rogue10m/VFX/Character/Source` | 2종 | 소프트 먼지 Cascade 원본과 변환 Niagara |

## 공격 파티클 튜닝

| 사용처 | 스케일 | 방출 시간 |
|---|---:|---:|
| 기본 1타 | 0.22 | 0.10초 |
| 기본 2타·점프 기본 | 0.24 | 0.11초 |
| 기본 3타·특수 | 0.26 | 0.12초 |
| 차징 방출·점프 특수 | 0.32 | 0.16초 |
| 차징 유지 | 0.18 | 입력 유지 중, 해제 시 Deactivate |
| 적중 | 0.28 | 0.14초 |

공격 효과에는 Niagara Custom Time Dilation 2.0을 적용한다. Cast와 Impact는 AutoRelease 풀링과 약한 참조 Timer로 방출을 종료하며 Tick을 추가하지 않았다.

## 정리 처리

UE 5.8 명령렛에서 이전 공격 Niagara 6종의 참조자 0개와 새 위치의 대체 자산 존재를 확인했다. 명령렛의 Force Delete가 메모리에는 반영됐지만 원본 디스크 파일을 남기는 동작이 있어, 해당 파일은 삭제하지 않고 `tmp/legacy-character-vfx-backup-20260814`로 이동했다. 필요하면 원래 위치로 복구할 수 있다.

## 검증

- UE 5.8 `Rogue10mEditor Win64 Development` 전체 빌드 성공
- 최종 증분 빌드 성공
- 구성 스크립트: `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
- 검증기: `RESULT=PASSED`
- 검증 범위: Niagara 12종, 이동 6종/공격 6종 분류, 공격 8개 Data Asset 참조와 튜닝, 종족 6종, 점프 프로필 1/2
- 변환기 런타임 패키지 의존성 0개
- 기존 활성 평면 경로 Niagara 0개
- `CheckGeneratedChanges.ps1`, `git diff --check` 수행

## 관련 문서

- `Feature/architect/2026-08-14_organized-character-vfx.md`
- `Feature/doc/2026-08-14_organized-character-vfx.md`
- `DevLog/20260814.txt`
