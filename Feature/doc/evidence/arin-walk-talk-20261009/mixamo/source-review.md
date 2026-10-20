# Mixamo animation reference acquisition — 2026-10-09

공식 Mixamo 라이브러리에서 **Walking / Feminine Walk Forward**와 **Talking / General Conversation** 프리뷰를 실제 브라우저로 확인했다. 걷기는 30 프레임, 대화는 309 프레임으로 표시되었다. 해당 숫자는 프리뷰 UI의 길이이며, 다운로드한 FBX나 엔진의 검증값이 아니다.

- [Walking 검색](https://www.mixamo.com/#/?page=1&query=walking&type=Motion%2CMotionPack): 교대 발 착지와 반대팔 스윙, 작은 몸통 회전이 관찰되었다. In Place 옵션이 있다.
- [Talking 검색](https://www.mixamo.com/#/?page=1&query=talking&type=Motion%2CMotionPack): 발을 고정하고 한손 설명 제스처, 팔꿈치와 양손 몸짓, 약한 고개·몸통 움직임이 관찰되었다.

현재 Edge의 Mixamo 세션은 24시간 이상 비활동으로 로그아웃되어 있다. Walking 다운로드 버튼을 눌렀을 때 **Please sign in to download**가 표시되었다. 실제 다운로드한 애니메이션 파일은 **0개**이며, 로그인·계정 생성·캐릭터 업로드·결제는 수행하지 않았다. 공개 프리뷰를 참조한 자체 키프레임 제작은 가능하지만, 이를 Mixamo 모캡 다운로드나 리타깃 완료로 표현하면 안 된다.

Adobe 공식 [애니메이션 사용 안내](https://helpx.adobe.com/creative-cloud/help/animate-characters-mixamo.html)는 모션 검색, 프리뷰, 속도와 팔 간격 조절, 다운로드 절차를 설명한다. [공식 FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)는 Adobe ID로 무료 사용 가능하며 비디오 게임을 포함한 개인·상업·비영리 제작물에 캐릭터와 애니메이션을 사용할 수 있다고 설명한다. [Mixamo 추가 약관](https://wwwimages2.adobe.com/content/dam/cc/en/legal/servicetou/Mixamo-Addl-Terms-en_US-20210623.pdf)도 확인했으며 신규 동의는 하지 않았다.

프로젝트에는 `MF_Unarmed_Walk_Fwd`, `MM_Walk_InPlace`, `MM_Walk_Fwd` 파일이 이미 있다. 파일 존재만 확인했으므로, 재사용 전 실제 스켈레톤과 모션은 Editor에서 읽어야 한다.

현재 아린의 23본 리그에는 몸통·팔·다리·목·머리 체인은 있지만 개별 손가락·턱·얼굴 본은 없다. 따라서 기본 Talking은 대화 몸짓으로 제작하고, 음성 립싱크와 혼동하지 않는다. 실제 Mixamo FBX가 확보되면 본 의미 매핑과 rest basis 보정을 적용하고 기존 아린 스켈레톤·스킨 가중치를 유지해야 한다.
