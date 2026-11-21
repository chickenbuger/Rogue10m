# 아자토르 보행 접촉 보정

아자토르가 이동할 때 애니메이션의 전진 보폭을 현재 수평 이동 속도에 맞췄다. Garret 메시의 활성 `A_HumanBoss_Walk` 클립을 60Hz로 읽어 지지 발의 속도 중앙값 300.07cm/s를 측정했다. 기준 속도를 아자토르 프로필에서 300cm/s로 설정했다. 실제 240cm/s 보행에서는 0.8배로 재생한다. 기존 리타깃 클립의 루트 잠금과 공격 몽타주 타이밍을 사용한다. [활성 에셋 측정](evidence/azathor-walk-contact-20261121/walk-asset-reference.json) · [설계](../architect/2026-11-21_azathor-walk-contact.md)

StartHub의 실제 보스 프로필과 충돌 바닥에서 적용 전후를 비교했다. 보행 입력과 측정 시간은 프로그램으로 제어했다. 같은 클립과 이동 속도에서 이전 고정 재생은 73개 지지 표본의 진행축 미끄러짐 속도 중앙값이 59.446cm/s였다. 새 재생은 79개 지지 표본에서 2.184cm/s였다. 기존 동작을 재현한 1배 설정은 검증 세션의 임시 프로필에서 실행했다. 이번 비교는 스켈레탈 발 본의 좌표를 측정했다. [검사 결과](evidence/azathor-walk-contact-20261121/walk-contact-results.txt) · [발 궤적](evidence/azathor-walk-contact-20261121/walk-contact-before-after.csv)

아린 전환과 근접 전투 효과를 포함한 통합본에서 UE 5.8 Editor 빌드를 통과했다. 보행 41개 검사와 보스 전투 18개 시나리오의 401개 검사가 모두 통과했고 두 런타임 로그의 신규 오류는 0건이었다. 최초 헤드리스 전투 검사는 렌더러가 필요한 첫 캡처 준비 검증에서 중단됐다. 렌더 실행에서 그 검사를 유지하고 헤드리스에서는 보스 전투 케이스를 실행하도록 검증기를 고쳤다. [최종 전투 회귀](evidence/azathor-walk-contact-20261121/boss-regression-results.txt) · [최초 실패 기록](evidence/azathor-walk-contact-20261121/regression-first-attempt.txt)

재생률 코드의 기본값은 기존 프로필의 속도 동작을 유지한다. 활성 프로필의 변경은 Unreal Editor에서 저장했다. 라이브 에디터 재시작과 본 보행의 시각 검증은 별도로 남아 있다.
