# 정상 경로 자동 플레이 검증 도구

계획은 Feature/architect/2026-11-01_basic-run-playtest.md, 실행기는 Scripts/VerifyBasicRunPlay.ps1, 관찰기는 Tests/Rogue10mBasicRunPlaytest.cpp다. exactfresh3슬롯/전후savehash/자기PIDwatchdog/현재UIvisible+enabled확인을사용하며 HP/XP/위치/시간/무적을주입하거나직접진행API를호출하지않는다. ControllerInputKey를여러프레임유지하고DoAim/Slate문자/실제UMGdelegate로진행한다. 사람의물리입력·UIhit-test와구분한다.

매번새Label: `.\Scripts\VerifyBasicRunPlay.ps1 -Label <fresh>`. 결과는DevLog/BasicRunPlayQA/<Label>에보존하고Git추가에서제외한다. summary의outcome/full_boss_completion/각milestone을함께읽는다. 프로세스exit0만으로보스승리를주장하지않는다. whilegame중인CSV를열지않고종료후해석한다. 단계PNG는애니메이션준비/접촉/회복세장인증을대신하지않는다.

A/B는NPC안내소실, C읽기계측과D생명주기추적이원인을좁혔다. NPC수정후E/F/G는실제획득·전투·학습을이어갔다. E는road경사교차지점, F/G는보스보호floor아랫면끼임으로미완료이며실패파일을삭제하지않았다. 별도Editor읽기자료V1trace는Python nativeBreak호출불가여서좌표미검증, V2to_tuple로지형경사를읽었지만Editorstatic-collision조회는실행world충돌을배제하는근거가아니다. 최종판정은G실제ceiling hit다.

빌드: 첫새테스트32.61초실패(구조체헤더누락), 수정10.69초성공; NPC접근조건10.56, 읽기계측11.53, 생명주기추적12.47, NPC제품수정8.15, 경계/경로20.01, 이동관찰11.57초성공. 마지막관찰코드Radius이름shadow경고가있지만컴파일/실행은성공했다. 신규성능향상률이나FPS보장은없다.


## NPC 기능 체크포인트 및 정상 루프 진단 확정

NPC44검사와 핵심6suite30검사를 모두 통과했다. 정상 E/F/G는 메뉴→캐릭터→마을F대화/보급→도보포털→대검overlap→자연처치XP→K포인트학습을 통과했으나 보스 접근은 미완료다. G의 실제 Pawn 천장 line은Z9500.998에서보호바닥아랫면에start penetration/normalZ=-1을기록했다. 이동모드Walking/MaxWalkSpeed620/CanJump1/RootMotion0이므로입력·스태미나잠금으로해석하지않는다. E의별도위치는보호바닥밖이라같은원인으로합치지않는다.

시작실제세이브321개SHA와원본/팔31패키지SHA는전부같다. 현재실제세이브338개이며새QA만추가했다. 맵·원본자산·저장스키마변경없고원격작업없다. 아직실제보스전종료/결과/마을복귀를일반입력한판에서검증하지못했다. 다음은기존보스추락방지바닥을유지한채좁은남문보행경사로를runtime에만추가하는가역후보이며현재적용하지않았다.
