# 파진각의 낮은 전방 발차기

너클 수련 트리의 파진각이 주먹으로 올려치던 자세를 오른발을 낮게 뻗는 동작으로 교체했다. 왼발로 몸을 받치고 차는 무릎을 당긴 뒤 앞쪽으로 발을 내보낸다. 양손은 가드를 유지하고 발을 다시 바닥에 놓는다. Blender에서 프로젝트의 기존 Manny 준비 자세를 사용해 이 동작을 제작했다. 다른 기술이 사용하는 기존 어퍼컷 몽타주와 원본 시퀀스는 보존했다. [작업 계획](../architect/2026-11-21_breaking-kick-motion.md)

실제 전수 항목 `F.BreakingKick`의 `DA_Attack_Knuckle_Special`에 `AM_Fighter_BreakingKick`과 그 시퀀스를 연결했다. `DefaultSlot`과 1.666667초 구간, 0.21초 선행 지연, 재생 비율 1.14, 165cm·88° 범위와 1회 타격은 기존과 같다. 새 시퀀스의 89개 본과 60Hz 100구간을 Blender에서 다시 가져와 확인했다. 원본과 재가져온 본 위치의 최대 차이는 0.00014cm이고 루트 이동은 0cm다. [Blender 재가져오기](evidence/breaking-kick-motion-20261121/fresh-blender-reimport.json) · [Unreal 반입과 필드 검증](evidence/breaking-kick-motion-20261121/unreal-import.json)

## 실제 전수 무공으로 비교

StartHub의 같은 평지에서 수련용 너클을 인벤토리로 장착하고, 기존 수련 트리에서 전수된 파진각을 Q에 배치했다. 프로그램이 실제 컨트롤러로 Q를 보냈다. 정면 110cm의 훈련 표적에 두 버전 모두 같은 74프레임, 몽타주 0.241680초에 첫 피해가 한 번 들어갔다. 승인 관측부터 0.183333초였고 캐릭터 캡슐 이동은 0cm였다.

| 첫 피해의 표시 자세 | 기존 | 수정 후 |
| --- | ---: | ---: |
| 오른쪽 발볼의 바닥 위 높이 | 4.19cm | 54.55cm |
| 오른쪽 발볼과 표적 캡슐 표면 간격 | +133.36cm | −3.53cm |
| 10프레임 전부터 오른쪽 발볼 이동 | 12.25cm | 130.49cm |
| 같은 구간 오른손 이동 | 59.20cm | 47.73cm |

음수 간격은 표시 발볼 본이 시험용 표적의 충돌 캡슐에 들어간 것이다. 실제 스킨 표면이나 판정 외곽 전체의 접촉은 이 수치와 구분한다. 클립 안에서 왼발볼의 이동은 0.00006cm였다. 실제 게임에서는 몽타주가 직전 대기 자세와 섞이는 시간이 있으므로 이 값과 본 좌표의 비교 구간을 따로 기록했다. [게임 샘플과 비교](evidence/breaking-kick-motion-20261121/comparison.json) · [선택한 양손·양발 프레임](evidence/breaking-kick-motion-20261121/pose-comparison.csv)

기준본은 `5d125c1e`, 수정본 수치는 `55ab2ddc`에 통합한 작업 사본에서 기록했다. 첫 발차기 후보는 발볼이 캡슐보다 9.56cm 짧았다. 최종 클립에서 상체 가드와 왼발 위치를 유지하며 오른발이 조금 더 뻗도록 다듬었다. 원래 몽타주의 트랙이 복제 시 남는 문제는 새 몽타주 내부에 시퀀스를 다시 기록하고 새 실행에서 확인했다. [후보와 오류 기록](evidence/breaking-kick-motion-20261121/iterations.txt)

720p 화면은 준비 61프레임, 첫 피해 74프레임, 회복 146프레임이다. 첫 피해 화면에서 왼발로 지지하며 오른발을 낮게 뻗고, 회복 때 오른발을 내려 양손 가드로 돌아온다. 정면 110cm에 둔 대상과 발의 윤곽 사이에는 화면상 간격이 남는다. 기존 근접 전투 14건도 통과했고 최종 검증 로그의 새 오류는 0건이다. [준비](evidence/breaking-kick-motion-20261121/ready.png) · [첫 피해](../../DevLog/20261121_BreakingKick/first-damage-720p.png) · [회복](evidence/breaking-kick-motion-20261121/recovery.png) · [검증 기록](evidence/breaking-kick-motion-20261121/validation.txt)

수치 재현은 `./Scripts/VerifyMeleeAnimationVfx.ps1 -Label NewBreakingKick -BreakingKick -ExpectAuthoredKick -LogicOnly`다. 720p 시험은 새 검사 이름을 쓰고 `-LogicOnly`를 빼면 된다. 입력은 격리 게임에서 프로그램으로 보냈다. 물리 입력, 필드 완주와 다른 외형의 화면은 시험 범위 밖이다.
