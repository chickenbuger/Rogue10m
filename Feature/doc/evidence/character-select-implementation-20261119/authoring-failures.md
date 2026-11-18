# 자산 적용 도구의 실패와 수정

## EditorA — 경로 인자

Unreal Python commandlet의 `-Script` 내부 인자에 Windows 역슬래시 경로를 사용해 `\t`가 탭으로 해석됐다. argparse가 `unrecognized arguments: mp\CharacterLobbyStyle20261119\EditorA`로 종료했다. 실제 로그의 신규 오류 16줄을 보존했다. 스크립트의 저작 단계와 자산 저장은 시작되지 않았다. `-Script` 내부 경로를 모두 `/` 표기로 바꿨다.

## EditorB — Python API와 중간 바인딩

`TextBlock.set_justification`은 현재 Python API에 노출되지 않아 `AttributeError`가 발생했다. 기존 스크립트와 같은 `set_editor_property("justification", ...)`로 변경했다.

동시에 전체 트리를 지우는 이전 저작 흐름에서 필수 BindWidget 누락 오류가 발생했다. 설치된 UE5.8의 `UMGToolSet` 및 `FWidgetBlueprintOperationUtils`를 읽어 Add/Remove가 구조 변경을 즉시 반영하는 것을 확인했다. 기존 root와 필수 위젯을 유지하고, 선택 노드만 추가하며, 이동이 완료된 뒤 구조 변경을 반영하는 공식 MoveWidget으로 이름표를 재배치하는 방식으로 설계를 바꿨다. 컴파일 로그를 끄거나 검사를 생략하지 않는다.

EditorB 로그의 신규 오류는 599줄이며 원문과 전체 파싱 기록은 `tmp/CharacterLobbyStyle20261119/EditorB/`에 남겼다. 실패 전후 원래 WBP의 SHA256은 `28192DEFA144495BDFF9C3A061C00F3C97B5A8B4BE5003990A18418B3C342AFD`로 같고, 실제 자산 저장은 없었다. 배경·카탈로그·투명 프리뷰 재질도 변경되지 않았다.

이 문서는 실패의 고유 원인과 보존 경계를 기록한다. 전체 RPC/세션 로그 배치를 개발 기록에 복사하지 않으며, 성공한 최종 실행의 결과는 별도 최종 검증 기록을 따른다.
