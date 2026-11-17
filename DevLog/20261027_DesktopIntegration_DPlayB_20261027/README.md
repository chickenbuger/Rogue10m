# D 프로젝트 렌더링 무기 교체 QA — DPlayB_20261027

작업 경로 `D:\Project\Rogue10m`, 브랜치 `Sprint#9-6-desktop-integration`, 반영 기준 커밋 `6246d60c7de4bd708172e1ed87bafa80f57b12f1`. 추가된 Editor 전용 QA 소스의 SHA256은 verification.json에 기록한다. 최종 기능 커밋은 후속 기록을 참조한다.

B 실행은34개 검사 통과. 실제 타입과 아이템을 대조하고 HUD 갱신 뒤 프레임을 촬영했다. 이 폴더가 최종 권장 렌더링 증거다.

이 사진6개는 실제 D 프로젝트의 Unreal D3D11 게임 뷰포트다. 게임 실행 중 FScreenshotRequest로 원본 PNG를 기록했으며 합성/생성/리터치를 하지 않았다.1280x720 PNG6개 모두 디코딩·열기 검증 통과. 검증 시각은 verification.json, 단계별 결과는 자동화 원본 results.txt에 있다.

범위: StartHub의 새 QA 프로필, 저장 연결 분리 후 임시 소유 무기2개, AI 중지·보상 제외된 실제 몬스터, 실제 PlayerController InputKey 이동/공격 경로. 공격 중 거부·달리기 교체·0.5초 하한·긴 쿨타임 보존·실제 체력 감소를 확인했다.5초 쿨타임은 QA 중 메모리 임시값이며 원래 값으로 복원했다.

물리 키보드/마우스 수동 플레이는 확인하지 못했다. 데스크톱 캡처 도구가 게임 대신 배경을 반환하고 Windows 키 유지 입력을 지원하지 않았다. 수동 단계 지표는0이다. 본 결과는 프로그램 주도 렌더링 QA이며 일반 완주/20분 생존/보스 밸런스/60초 이동 경로 검증이 아니다. 사망은 상태 플래그 fixture이며 실제 사망 연출 검증이 아니다.

재현: `Scripts/VerifyDesktopIntegration.ps1 -Label 새고유라벨 -NativeHoldSeconds 0 -ProjectRoot D:\Project\Rogue10m`. 기존 증거/QA 슬롯이 있으면 다른 라벨을 사용한다. 프로필/성장/키 슬롯은 모두 QA 이름으로 강제되고 실제 사용자 슬롯을 사용하지 않는다. 기존 세이브98개 해시 차이0.

미디어와 원시 로그는 로컬 보존 전용으로 Git에 추가하지 않는다.

최종 로컬 기능 커밋: `335655ab047013a1117355dffaca88d633d5987e`. GitHub 새 브랜치 push는 자동 승인 검토에서 실행 전 차단되어 미게시 상태다.

