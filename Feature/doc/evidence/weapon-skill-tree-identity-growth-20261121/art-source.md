# 무기 스킬트리 아트 원본

사용자가 지정한 공통 게임 화면을 [기준 이미지](common-reference.png)로 보존했다. 같은 이미지를 입력으로 사용해 [너클·검 배치 시안](weapon-concept.png)과 [메달 원본](../../../../Art/UI/CommonSkillTree/WeaponMedallions.generated.png)을 ImageGen으로 만들었다.

시안에는 어두운 숯색 패널, 얇은 황동 테두리, 금속 이름표, 반짝이는 연결선과 상세 메달을 요청했다. 너클은 기초 네 개에서 상단으로 올라가는 수련형, 검은 중앙에서 여러 경로로 뻗는 거미줄형으로 지정했다. 실제 화면은 기존 Slate 공통 스타일과 런타임 노드 배치를 사용한다.

메달은 하이릴리프, 얇은 이중 금속 테두리, 원형 검은 바탕을 요청했다. 원본의 행별 문양은 왼손 잽·오른손 잽·스트레이트·훅, 장권·유운수·어깨 타격·불꽃 쌍권, 대검·분쇄참·회전참·파쇄참, 대지파쇄·패왕참·방패·공명 문장이다. `PackWeaponSkillMedallions.ps1`로 4×4 UV 격자에 정렬하고 코인 바깥의 발광 잔여물을 마스킹했다. `ImportWeaponSkillTreeArt.py`를 Unreal Editor에서 실행해 `/Game/UI/CommonSkillTree/T_WeaponSkillMedallions`로 가져온다.
