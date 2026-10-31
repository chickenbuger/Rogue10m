# 소유자 팔 프레이밍 보정

기준7958b621,새Sprint#9-15-owner-view-framing. 제품소유자시점은기본OFF유지. 현재1127파일/세이브270을복사·SHA256검증했다. 기존6개팔·원본body/skeleton/material과이전43사진을보존한다.

## ULW 작업 묶음

1. 진단: QA-only fresh프로필정상입장후detach. 팔geometry ON/OFF를view hidden primitive로분리하고, 월드몸체shadow ON/OFF를별도비교. 실제CPUskinnedvertex/triangle의camera space/near교차를기록한다. 정지0/-85/벽근처와기존대시캡처를대조한다. 동일pose를단정하지않고프레임차·camera/pose metadata를남긴다.
2. 보정: 진단결과에따라owner-only presentation을가역적으로보정한다. 원본mesh/animation,worldbody/source pose,camera/combat trace,hit clock,collision/save는변경하지않는다. 이미baked된upperbody viewaim을doublepitch하지않는다. 제품defaultOFF유지, 조절가능수치는후보로기록한다.
3. 검증: 실제Original/Weighted렌더에서전후사진과skinnedprojection을검토하고해당QA·관련기존회귀를수행한다. 시각실패를수치PASS로바꾸지않는다. 물리입력/필드완주/packaging을주장하지않는다.
4. 기록/종료: Feature/doc,DevLog/20261031.txt,기능별전후사진·준비/피해관측/회복/궤적및Sprint기록. 로컬검증체크포인트만,원격push/병합없음.

롤백은신규브랜치/신규소스변경경계로관리하며기존파일reset/삭제/자동병합은없다. shadow분리와정점분석을완료하기전눈대중camera후퇴나팔전진수치로강행하지않는다.

보존 manifest: C:\Users\PC\Documents\Codex\2026-10-27\task\preservation-20261031-165446\manifest.json
