# UE 5.8.4 한국어 시작 오류 분석

## 확인한 증상
- 대상: DevLog/20261110_HumanoidBossNpc/NPC-live-editor.log.
- Error 수준 로그 21줄: 엔진 시작 시 Condition failed 15줄, 과거 Python 예외 6줄.
- 엔진은 UE 5.8.4, CL 58902052이며 에디터 언어 ko, 로캘 ko-KR였다.

## 원인
1. Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp 479–509, 512–533의 11개 CHECK가 현지화된 결과를 영어 고정 문자열과 비교한다.
2. Core/Tests/Logging/StructuredLogFormatTest.cpp 183/192, 266의 비교가 237, 276, 314에서 호출되어 4개가 추가로 실패한다.
3. 설치된 Engine/Content/Localization/Engine/ko/Engine.locres에는 Empty error, Error with int, Target 및 Found errors 관련 한국어 번역이 실제 포함돼 있다.
4. Launch/Private/LaunchEngineLoop.cpp 4374–4376이 시작 smoke test를 실행한다. Core/Public/Misc/LowLevelTestAdapter.h 130에서 CHECK 실패가 일반적인 Condition failed로 바뀐다.
5. Core/Private/Misc/AutomationTest.cpp 558–559는 smoke test 스택 수집을 끄고 592에서 결과를 출력하므로 출력 창만 보면 원인 파일이 드러나지 않는다.

11 + 4 = 15로 실제 오류 수와 일치한다. 이 오류는 의도된 정상 출력이 아니라 엔진 테스트의 현지화 호환 결함이다. NPC 게임플레이 실패를 의미하지는 않는다. 직전 UnifiedError LogTemp 5줄은 같은 테스트 파일 973–997에서 정상 Log 수준으로 출력하는 별도 검증 내용이다.

## 좁은 검증 방법
- 후속 QA 프로세스에만 -language=en -locale=en-US를 전달한다.
- 사용자 에디터의 한국어 설정·번역 파일·로그 심각도는 변경하지 않는다.
- smoke test를 끄거나 Error 카테고리를 숨기지 않는다.
- 다음 QA 로그에서 Condition failed가 사라졌는지, 테스트와 실제 게임플레이가 모두 통과했는지 별도로 확인한다. 아직 재실행 전에는 해결 완료로 기록하지 않는다.
- 엔진 자체의 근본 수정은 해당 테스트 3개에만 영어 culture backup/restore 범위를 두는 방식이다. Launcher 엔진 원본은 이 작업에서 변경하지 않는다.

## Python 예외
- PreviewHumanBoss20261110.py의 get_capsule_component()는 Python에 노출되지 않아 실패했다.
- 현재 get_editor_property('capsule_component')로 수정돼 있으며 같은 로그의 후속 실행에서 capsule 88/34 검사에 성공했다.
- 이후 Error, Fatal, Ensure 추가 발생은 없고 정상 종료 로그가 확인됐다.

## 추가 프리뷰 결함
- Python Rotator 위치 인자 순서로 인해 프리뷰만 -90도 pitch로 눕는 오류가 있었다. pitch/yaw/roll 키워드로 수정했다.
- AlwaysTickPoseAndRefreshBones, 활성 editor animation Tick, 일시적인 background throttle 해제로 실제 포즈 갱신을 확인했다.
- 실패한 PreviewA/B를 덮어쓰지 않았고 올바른 자세의 PreviewC를 별도 저장했다. 이 교정은 라이브 보스 자산이 아니라 임시 프리뷰 경로의 수정이다.

## 재검증
- boss-activation-B.log에서 영어 프로세스 언어/로캘만 사용하고 smoke test를 유지한 실행: exit 0, RESULT=HUMAN_BOSS_ACTIVATED, Error/Condition failed 0건.
- 초기 활성화 A의 AlphaBlendArgs/AlphaBlend 타입 오류는 기존 UE5.8 성공 스크립트와 같은 get/set blend_time 방식으로 수정했다. B 실행에서 같은 오류 없이 자산 저장이 완료됐다.
- 이는 한국어 엔진 smoke test 자체를 수정한 것이 아니다. 한국어로 다시 시작하면 해당 엔진 테스트 결함이 남는다.

## -game에서 에디터 도구 Python 초기화 오류
- 실제 3인칭 게임 실행 TP_A1에서는 NiagaraToolsets/EditorToolset의 init_unreal.py가 Editor 전용 unreal.AgentSkill을 참조해 AttributeError를 냈다. 이는 언어 비교 오류와 별개의 오류다.
- 플러그인 EditorToolset.uplugin의 모듈은 Type=Editor, TargetAllowList=Editor이며 -game에서는 해당 에디터 클래스가 없는 반면 Python 콘텐츠 초기화는 수행됐다.
- 공식 PythonScriptPlugin.cpp 116–119에 지원된 -DisablePython을 순수 C++ -game QA 실행기에만 적용한다. 런타임 게임에 필요 없는 에디터 스크립팅 초기화를 하지 않도록 분리하는 조치이며 로그 카테고리를 숨기지 않는다.
- 에디터/MCP와 자산 제작 Python commandlet에는 해당 플래그를 사용하지 않는다. 사용자 플러그인 설정·엔진 원본은 변경하지 않는다.
- 플래그 적용 후 실제 재실행 결과로 최종 오류 수를 확인한다.
