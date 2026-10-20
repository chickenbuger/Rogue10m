# 길잡이 아린 — Blender 입체 모델

시작마을 튜토리얼 안내인 콘셉트를 바탕으로 Blender에서 몸·얼굴·머리·의상·망토·가방·물약을 부위별 입체 메시로 제작한 스타일화된 프로토타입이다. 참조 그림을 메시 표면에 투영하지 않았다.

- `Arin.blend`: 편집 가능한 부위별 메시·23본 리그·Idle·실제 렌더 스튜디오.
- `Arin.fbx`: Skeletal Mesh와 Idle을 함께 전달하는 FBX.
- `SK_Arin.fbx`: T포즈 바인드 기준의 Skeletal Mesh FBX.
- `A_Arin_Idle.fbx`: 같은 Skeleton용 Idle 전용 FBX.
- `Arin.glb`: 재질과 애니메이션을 포함한 GLB.

미터 단위, 원점은 발바닥, 신장170cm, Blender 좌표 Z up/front -Y. FBX는 -Z forward/Y up 축 변환과 unit metadata를 포함한다. Idle은 30fps,1~61프레임,2초 루프다. 골격은 아린 전용이며 기존 Manny/Quinn Skeleton을 사용하지 않는다.

모델은 색상 재질을 사용한다. 얼굴 표정·손가락·입술 립싱크용 본/모프, 보행·공격 애니메이션, LOD·출시 성능 최적화는 이 프로토타입에 포함하지 않았다. 원본 콘셉트 이미지와 동일한 얼굴을 재구성했다고 주장하지 않는다.

제작 소스: `Scripts/Blender/ArinHead.py`, `ArinBody.py`, `BuildArin.py`. 실제 FBX 재수입·웨이트·Idle 검증과 렌더 근거는 `Feature/doc/evidence/arin-blender-20261007/`에 보존한다. 게임 적용과 최종 검증 상태는 `Feature/doc/2026-10-07_blender-arin-npc.md`를 확인한다.
