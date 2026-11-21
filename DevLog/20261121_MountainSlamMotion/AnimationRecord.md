# 태산압정 공중 자세 기록

- 프로젝트 자료 DA_Attack_StoneFist_JumpSlam. 이전 자세의 Manny A_Safe_Hook과 현재 MM_Fall_Loop를 Unreal Editor에서 참조용 FBX로 내보냈다. Blender 4.5.14에서 같은 골격의 상체 가산 클립을 저작했고, 실제 태산압정에 AM_Fighter_MountainSlam을 연결했다.
- A_Fighter_MountainSlam_Additive는 60Hz 100구간, 1.666667초다. 정지된 기준 프레임 0을 사용한다. 루트·골반·다리의 키 차이는 0이다. 소스와 반출본 본 위치 최대 오차는 0.00012cm였다.
- StartHub에서 수련용 너클·공중 제어·이단 점프·태산압정을 실제 게임 경로로 적용했다. 한 번과 두 번의 Space 뒤 Q를 넣었다. 시각 판독은 기본 인간 남성의 프로그램 입력 720p 화면이다. 첫 피해, 10프레임 뒤 내려치기, 접지를 구분했다. [검증과 자료](../../Feature/doc/2026-11-21_active-skill-motion.md)
