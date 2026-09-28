# Martelo 1인칭 프리뷰 독립 소스 검토

최종 상태: **소스 검토 통과**. 이번 범위에서 남은 차단 사항은 없다. 아래 구현과 실제 검증 로그를 확인했다. 시각적 완성도는 별도 `visual-review.md`의 판단 범위이며, 이 결과는 정식 공격 입력·피해 연결의 구현 완료를 뜻하지 않는다.

## 확인 범위

- `Scripts/Editor/PreviewMarteloRetarget.py`
- `Scripts/Editor/ValidateMarteloPreview.py`
- `Source/Rogue10m/Tests/Rogue10mMarteloPreviewRuntimeTest.cpp`
- `tmp/martelo-game/source_bind_proxy.cpp`
- UE 5.8 로컬 AnimPose, IKRetargeterController, 자동 리타깃 생성 구현
- `proxy-motion-comparison.json`, `reuse-comparison.json`, `preview-cleanup.json`
- 최종 `assets-persistent.log`, `assets-reuse.log`, `runtime.log`

## 원본 보존과 리타깃 경계

프록시는 사용자 FBX를 읽어 별도 파일에 65개의 작은 가중 삼각형을 추가하고, FBXSDK_TIME_INFINITE의 정적 변환으로 bind pose를 만든다. 재생 0프레임을 기준 포즈로 사용하지 않는다. Mixamo namespace만 제거하며 애니메이션 곡선을 직접 수정하지 않는다. 별도 비교 자료에서 40프레임 × 65뼈의 원본·프록시 global position 최대 차이는 0cm였다. 이는 위치 보존 검증이며 회전의 별도 전수 비교를 뜻하지 않는다.

현재 생성 도구는 소스 메시·스켈레톤·애니메이션을 `/Game/Rogue10m/Animation/Preview/Martelo/Source/Persistent`에 저장한다. 새 IKRig·Retargeter·타깃 AnimSequence도 Martelo 프리뷰 경로 안에 있다. save 함수는 이 경로 밖 저장을 거부한다. 기존 Manny는 참조·읽기 대상으로만 사용하고, 기존 스켈레톤 기준 포즈 갱신 옵션은 꺼져 있다. 기존 공격 자산·프로필을 수정하는 코드가 없다.

## 확인된 문제와 해결

1. **유한하지 않은 포즈가 검증을 통과할 가능성**: 타깃 포즈의 위치·회전·스케일에 유한성 검사를 추가했다. bone 목록, 타깃 스켈레톤, 길이, 왼발 높이 변화도 별도로 검사한다.
2. **프리뷰 종료 수명**: World cleanup에서 tick/cleanup delegate를 제거하고 고정 시간 및 자신이 추가한 입력무시 stack을 복구한다. GIsEditor를 거부하며 EWorldType::Game에서만 시작해 PIE에서 프로세스 종료 명령이 실행되지 않는다.
3. **반복 리타깃 연산 누적**: 연산 전체 제거 → 기본 연산 추가 → 정확한 chain map → 현재 타깃 pose reset → auto-align으로 수정했다. 엔진의 자동 리타깃 생성 흐름과 일치한다.
4. **소스 의존 자산 저장 누락**: 최초 가져오기에서 주 메시만 저장되어 이후 프로세스에 소스 스켈레톤·애니메이션이 없었다. 새 Persistent 경로에서 skeleton → animation → mesh를 명시적으로 저장하고 실제 파일 존재를 검사하도록 수정했다.
5. **반복 가져오기의 기준 포즈 변경 위험**: 기존 불완전 소스에 대한 반복 가져오기에서 Interchange의 time-zero 재바인딩 경고가 확인됐다. 현재는 정확한 세 자산 경로, 같은 skeleton, 정상 길이와 원본·프록시 SHA256 metadata가 일치할 때 가져오기를 건너뛴다. 부분 저장·해시 불일치는 덮어쓰지 않고 중단한다.

기존 실패 시 생성한 프리뷰 소스 메시 한 개는 referencer가 없음을 확인한 뒤 Editor API로 정리했다. `preview-cleanup.json`에 경로, 빈 referencer 목록, 삭제 성공이 기록되어 있다. 실제 디스크에는 Persistent 소스의 메시·스켈레톤·애니메이션 세 자산만 남은 것을 확인했다. 게임 원본 자산은 정리 대상이 아니다.

## 독립 검증의 의미

AnimPoseSpaces.WORLD는 엔진 계약상 스켈레탈 컴포넌트 공간이다. optional Manny 메시가 모든 ref bone을 RequiredBones로 구성하며, 검증 도구는 반환 bone 목록을 확인한다. 왼발 높이 변화 검사는 타깃이 전부 reference pose로 생성된 경우를 거부할 수 있다.

런타임은 실제 팔 전용 메시에서 별도 SingleNode 애니메이션을 재생한다. 초기 상태를 기본 FistAnimInstance로 제한하며, 메시·시야 설정·조준 회전·기본 공격 AcceptedSerial이 변하지 않는지 검사한다. 재생 후 원래 애니메이션 모드와 클래스가 복귀하는지 확인한다. SingleNode에서 기존 FistAnimInstance 카메라 연동이 꺼지므로 최종 영상은 고정 시야 비교이며 이 제한을 로그에 명시한다.

## 최종 실행 증거

- Persistent 복구 실행: `RESULT=MARTELO_RETARGET_POSES_PASSED`, `RESULT=MARTELO_ASSETS_PASSED` 확인.
- 새 Editor 프로세스: `MARTELO_SOURCE_REUSED` 및 두 PASSED 확인. 원본·프록시 해시를 검사한 뒤 소스 재가져오기를 생략했고 영속 자산 세 개를 로드했다.
- 새 프로세스 재베이크 전후 비교: 길이 동일, 40개 평가 포즈의 최대 축 방향 위치 차이 **0.3379117cm**. 완전한 비트 동일성이나 포즈 완전 동일성을 주장하지 않는다.
- 최종 왼발 수직 변화 **136.7308cm**, 오른발 **7.2282cm**로 왼발 높은 발차기 흐름이 유지된다.
- 최종 시퀀스를 사용한 실제 게임 재촬영: `RESULT=MARTELO_PREVIEW_PASSED frames=209 failures=0`.
- 실제 팔 메시의 손 이동 거리: 왼손 **77.254cm**, 오른손 **101.540cm**. 길이 **1.30초**, 고정 시야. 기존 기본 공격으로 실행되지 않았음과 기본 애니메이션 복구 검사를 통과했다.

원본 전신 발차기가 팔 전용 메시에서 얼마나 잘 전달되는지는 기술적 통과와 별개다. 이번 소스 검토는 실제 적용 상황을 확인하는 독립 프리뷰라는 범위에서 통과이며, 정식 발차기 스킬·다리 표시·카메라 연출·공격 판정은 별도 작업이다.
