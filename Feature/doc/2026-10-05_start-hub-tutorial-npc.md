# 시작마을 안내 NPC와 출발 보급품

## 구현 내용

시작마을의 안내인과 F로 대화하고 출발 보급품을 받을 수 있다. 대화 카드에는 20분 탐험이 북쪽 포탈을 통과한 뒤 시작된다는 안내와 I/K 메뉴 설명을 표시한다. E 회피 키는 유지한다.

로직은 `ARogue10mTutorialNPC`, 문구·외형·보상은 `URogue10mTutorialDataAsset`으로 분리했다. 보급품은 기존 인벤토리의 실제 NxM 그리드에 들어가며 수령 메시지와 기존 획득 알림이 함께 동작한다. PlayerState의 수령 기록 컴포넌트와 GameInstance의 세션 기록이 반복 수령을 막는다. 가방 공간이 부족하면 수령 완료로 처리하지 않고 정리 후 재시도를 허용한다.

기존 포탈은 `OpenLevel`로 Pawn을 새로 생성하므로 보급품이 사라질 수 있었다. 전용 Subsystem이 지급 스택의 GUID와 실제 남은 수량만 포탈 직전에 확인하고 다음 맵으로 운반한다. 사용하거나 버린 보급품은 다시 생성하지 않는다. 일반 아이템·장비·전체 인벤토리 영구 저장은 기존 범위에 남겨 두었다.

NPC는 피해를 받지 않고 플레이어를 막지 않으며 Actor Tick을 사용하지 않는다. 근처에 로컬 플레이어가 있을 때만 짧은 타이머로 대화 범위와 화면 표시를 갱신한다. UI는 이동·시점·전투 입력을 가져가지 않는다.

## 검증 계약

`Scripts/Editor/ValidateTutorialNPC.py`는 별도 StartHub PIE에서 다음을 확인하도록 작성했다.

- 실제 보급품·외형·Idle 설정, F 대화 API, 입력 유지 및 비일시정지.
- 실제 지급 수량, 수령 기록, 반복 대화·재수령 차단.
- 가방 공간 부족 시 미수령 상태와 정리 후 재시도.
- 대화 범위를 벗어났을 때 종료, 마을 타이머 대기 유지.
- 실제 물약 한 개 사용 후 실제 포탈을 통과하고 남은 물약만 전달되는지 확인.

정적 검토와 Python AST 검사를 수행했다. 최종 UI 수정을 포함한 Editor 타깃 빌드가 성공했다(70.11초). 최신 빌드의 실제 StartHub PIE 재검증 57개가 모두 통과했다(`evidence/open-world-monsters-20261005/tutorial-runtime.json`). 실제 F 키 입력도 대화 열기 → 회복 포션 2개 수령 → 대화 닫기를 최종 확인했다(`guide-key-final.json`, status_result=passed). 대화·실제 지급·가방 공간 부족·재수령 차단·실제 물약 사용·실제 포탈 이동 후 남은 수량 전달·PIE 종료를 검증했다.

실제 화면 검증에서 NativeConstruct 이후에 WidgetTree를 만드는 방식은 최초 Slate 재구축에 반영되지 않는 문제를 확인했다. RebuildWidget에서 먼저 트리를 준비하도록 수정한 뒤 `guide-dialogue-play.png`의 실제 플레이 화면에서 한글 대화 카드가 보이는 것을 확인했다. `guide-reward-play.png`는 실제 F 키로 수령한 보급품의 안내 화면이다. 표시 검증은 이 실제 화면과 입력 결과를 사용하며, 화면이 보이는 상태에서도 0을 반환했던 UUserWidget 캐시 크기를 완료 조건으로 사용하지 않는다.

![시작마을 안내 NPC와 한글 대화 카드](D:/Project/Rogue10m/Feature/doc/evidence/open-world-monsters-20261005/guide-dialogue-play.png)

![실제 보급품 수령 안내](D:/Project/Rogue10m/Feature/doc/evidence/open-world-monsters-20261005/guide-reward-play.png)

## 적용 범위

현재 단일 플레이어 게임 세션을 기준으로 보급 기록을 보존한다. 영구 프로필 저장·네트워크 동기화·로그라이크 특성 강화 시스템은 이번 간단한 안내 NPC 범위에 포함하지 않는다.

## 길잡이 아린 시각 콘셉트 (2026-10-05)

사용자 요청에 따라 imagegen 내장 도구로 전신 콘셉트 한 장을 제작했다. 친근한 성인 안내인 표정, 초록 여행복과 짧은 망토, 가죽 보급 가방, 손에 든 회복 물약과 허리 주머니의 두 번째 물약으로 대화·출발 보급 역할을 표현했다. 목조 주택·푸른 기와·돌 우물·밝은 풀밭을 배경으로 시작마을 분위기를 맞췄다.

이미지는 `Feature/doc/evidence/start-hub-tutorial-npc-image-20261005/arin-concept-v1.png`, 실제 생성 프롬프트는 같은 폴더의 `prompt.txt`에 보존했다. 얼굴과 손의 자연스러운 형태, 물약 두 병, 양쪽 부츠까지 보이는 전신 구도와 전체 분위기를 직접 검토했다. PNG 크기는 1024×1536이며 원본과 프로젝트 사본의 SHA256 일치를 확인했다.

시각 콘셉트 제작과 문서 기록만 추가했다. Unreal 에셋·C++·설정 변경은 없으며 3D 모델 생성과 게임 내 메시 교체는 수행하지 않았다. 기존 기능 검사 결과를 유지하며 이번 이미지 작업에 빌드와 PIE를 다시 실행하지 않았다.

![길잡이 아린 전신 콘셉트](D:/Project/Rogue10m/Feature/doc/evidence/start-hub-tutorial-npc-image-20261005/arin-concept-v1.png)
