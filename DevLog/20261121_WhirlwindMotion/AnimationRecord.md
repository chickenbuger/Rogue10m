# 대검 회전참 동작 기록

- 격리 작업 사본 `GreatSwordTechniqueWorktree20261121`, `Sprint#25-2-greatsword-technique-motion`. 기준은 `ad94906d`, 실제 게임 기준과 수정본은 2026년 11월 21일에 같은 StartHub 60Hz 경로로 각각 확인했다.
- `A_GreatSword_AltSlash04Repair1`은 기존 프로젝트의 Mixamo 기반 대검 파생 클립이다. 시작과 끝은 `A_GreatSword_Idle`의 대검 자세를 가져왔다. Blender 4.5.14에서 골반과 IK에 기존 회전을 옮기고 빠른 구간의 시간을 늘렸다. 재반입한 FBX는 89개 뼈, 101개 키 시점, 60Hz였다. 뼈 위치 최대 오차는 0.000295cm였다. 원본과 반출본의 SHA256을 기록했다. [출처와 해시](../../Feature/doc/evidence/greatsword-technique-motion-20261121/fresh-blender-reimport.json)
- `DA_Hybrid_GreatSword_Whirlwind`에 `AM_GreatSword_Whirlwind`와 `A_GreatSword_Whirlwind`를 연결했다. 같은 몽타주 길이 1.666667초와 `DefaultSlot`, 진입·회복 0.25초 블렌드, 수락 틱의 1회 피해를 실측했다. 표시 어깨 방향은 15.42→409.37°, 검의 이웃 프레임 3D 각도는 16.43→12.25°였다. 캡슐의 위치와 방향 변화는 0이었다. [비교 수치와 타이밍](../../Feature/doc/2026-11-21_greatsword-technique-motion.md)
- 실제 렌더는 프로그램 Q 입력과 측면 카메라를 사용했다. 대표 화면은 회전참 수락·첫 피해 후 75번째 프레임의 후방 스윙이다. 1280×720 화면에서 두 손과 두 발, 검과 후방 표적을 확인했다. [대표 화면](rear-sweep.png)
