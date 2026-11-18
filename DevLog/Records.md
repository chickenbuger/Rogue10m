# 핵심 검증 기록

DevLog JSON326개 중 **55개(768,419바이트)**만 남겼다. 최종 판정·중요한 실패·에셋 출처가 없는 중복 세션과 촬영 메타데이터271개(8,023,339바이트)는 삭제했다.

보존한 JSON의 원본 값과 해시는 변경하지 않았다. 실패나 시각 검토 미완료를 성공으로 바꾸지 않았으며, JSON 안의 오래된 파일명은 당시 실행 이력일 수 있다. 현재 보관 여부는 [정리 목록](../Feature/doc/evidence/devlog-record-curation-20261118/record-manifest.csv)을 따른다.

기존 ignore 규칙에 해당하는 로컬 전용 QA 기록 7개는 보존하되 공개 링크와 추가 추적에서 제외했다. 이름이 captures.json이어도 유일한 주요 실패 원인을 담은 두 기록은 남겼다.

| 기능 묶음 | 보존한 핵심 기록 |
| --- | --- |
| 20261027_AtomicAttackCosts | [verification.json](20261027_AtomicAttackCosts/verification.json) |
| 20261027_DesktopIntegration_KnuckleInput_1029 | [verification_20261029.json](20261027_DesktopIntegration_KnuckleInput_1029/verification_20261029.json) |
| 20261027_ProgressionRequirements | [verification.json](20261027_ProgressionRequirements/verification.json) |
| 20261027_SharedAttackCooldown | [verification.json](20261027_SharedAttackCooldown/verification.json) |
| 20261027_WeaponSwitchSafety | [verification.json](20261027_WeaponSwitchSafety/verification.json) |
| 20261029_FirstPersonSettings_FpUiRenderC1029 | [verification.json](20261029_FirstPersonSettings_FpUiRenderC1029/verification.json) |
| 20261029_InputFrameValidation | [verification.json](20261029_InputFrameValidation/verification.json) |
| 20261029_LookDash_Review | [CHECKPOINT.json](20261029_LookDash_Review/CHECKPOINT.json) · [geometry-summary.json](20261029_LookDash_Review/geometry-summary.json) |
| 20261029_MenuAbandon | [verification.json](20261029_MenuAbandon/verification.json) |
| 20261029_RunDomains_DomainsTravelR_1029 | [verification_20261029.json](20261029_RunDomains_DomainsTravelR_1029/verification_20261029.json) |
| 20261029_RunResultUI_UIAbGainD1029 | [verification.json](20261029_RunResultUI_UIAbGainD1029/verification.json) |
| 20261029_RunResultUI_UICapD1029 | [verification.json](20261029_RunResultUI_UICapD1029/verification.json) |
| 20261029_RunResultUI_UIFailureD1029 | [verification.json](20261029_RunResultUI_UIFailureD1029/verification.json) |
| 20261029_RunResultUI_UINormalD1029 | [verification.json](20261029_RunResultUI_UINormalD1029/verification.json) |
| 20261029_WeightedFirstPerson_HeavyRenderE1029 | [verification.json](20261029_WeightedFirstPerson_HeavyRenderE1029/verification.json) |
| 20261031_OwnerFirstPerson_Review | [VERIFICATION.json](20261031_OwnerFirstPerson_Review/VERIFICATION.json) · [create-owner-arms-20261031-162857-778266.json](20261031_OwnerFirstPerson_Review/Assets/create-owner-arms-20261031-162857-778266.json) · [owner-animation-analysis-20261031.json](20261031_OwnerFirstPerson_Review/Evidence/owner-animation-analysis-20261031.json) · [owner-arms-reload-audit-20261031.json](20261031_OwnerFirstPerson_Review/Evidence/owner-arms-reload-audit-20261031.json) |
| 20261031_OwnerViewFraming_Review | [CHECKPOINT.json](20261031_OwnerViewFraming_Review/CHECKPOINT.json) · [VERIFICATION.json](20261031_OwnerViewFraming_Review/VERIFICATION.json) · [owner-framing-bounded-final1031.json](20261031_OwnerViewFraming_Review/Analysis/owner-framing-bounded-final1031.json) · [owner-framing-metadata-only-change.json](20261031_OwnerViewFraming_Review/Analysis/owner-framing-metadata-only-change.json) · [AttackResources.json](20261031_OwnerViewFraming_Review/Regression/AttackResources.json) · [HybridSkillTree.json](20261031_OwnerViewFraming_Review/Regression/HybridSkillTree.json) · [ProgressionRequirements.json](20261031_OwnerViewFraming_Review/Regression/ProgressionRequirements.json) · [RunCurrencyBaseline.json](20261031_OwnerViewFraming_Review/Regression/RunCurrencyBaseline.json) · [WeaponCooldown.json](20261031_OwnerViewFraming_Review/Regression/WeaponCooldown.json) · [WeaponSwitch.json](20261031_OwnerViewFraming_Review/Regression/WeaponSwitch.json) |
| 20261101_AltDodge_Review | [VERIFICATION.json](20261101_AltDodge_Review/VERIFICATION.json) |
| 20261101_BasicPlayLoop_Review | [VERIFICATION.json](20261101_BasicPlayLoop_Review/VERIFICATION.json) |
| 20261101_BossSouthApproach_Review | [VERIFICATION.json](20261101_BossSouthApproach_Review/VERIFICATION.json) |
| 20261101_LobbyTraitDisplay_Review | [VERIFICATION.json](20261101_LobbyTraitDisplay_Review/VERIFICATION.json) |
| 20261101_NpcDiscovery_Review | [VERIFICATION.json](20261101_NpcDiscovery_Review/VERIFICATION.json) |
| 20261101_PreparedRunInput_Diagnostics | [VERIFICATION.json](20261101_PreparedRunInput_Diagnostics/VERIFICATION.json) |
| 20261110_HumanoidBossNpc | [activation.json](20261110_HumanoidBossNpc/NPC/activation.json) · [adapted-report.json](20261110_HumanoidBossNpc/NPC/adapted-report.json) · [retarget-report.json](20261110_HumanoidBossNpc/NPC/retarget-report.json) · [runtime.json](20261110_HumanoidBossNpc/NPC/runtime.json) · [scale-diagnostic.json](20261110_HumanoidBossNpc/NPC/scale-diagnostic.json) · [manifest.json](20261110_HumanoidBossNpc/NPC/ViewportA/manifest.json) |
| 20261110_SwordKnuckle | [skill-profile-metadata.json](20261110_SwordKnuckle/PreviewAligned_61da49fe/skill-profile-metadata.json) · [visual-review.json](20261110_SwordKnuckle/SkillParticlesDiagnosticD/visual-review.json) · [captures.json](20261110_SwordKnuckle/SkillParticlesPreviewA/captures.json) · [captures.json](20261110_SwordKnuckle/SkillParticlesPreviewB/captures.json) · [visual-review.json](20261110_SwordKnuckle/SkillParticlesPreviewC/visual-review.json) · [visual-review.json](20261110_SwordKnuckle/SkillParticlesReviewG/visual-review.json) |

대표 사진은 [사진 목록](README.md)을 따른다. 영상·텍스트 로그·CSV·실제 세이브와 DevLog 밖의 canonical 검증 자료는 이번 정리 범위가 아니다.
