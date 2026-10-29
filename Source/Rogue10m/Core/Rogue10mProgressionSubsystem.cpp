// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mProgressionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mProgressionPersistence
{
static int32 TraitSpent(const FRogue10mTownProgressionSnapshot& Town)
{
	const auto Canonical = URogue10mTownProgressionComponent::MigrateLegacySnapshot(Town);
	int64 Total = 0;
	for (const auto& Investment : Canonical.TraitInvestments) { Total += Investment.SpentPoints; }
	return static_cast<int32>(FMath::Clamp<int64>(Total, 0, MAX_int32));
}
/** Version 2 mirrors its canonical records for compatibility with old callers. */
static bool HasCanonicalTraitMirrors(const FRogue10mTownProgressionSnapshot& Town)
{
	int32 VitalityRank = 0, PowerRank = 0;
	float VitalityBonus = 0.0f, PowerBonus = 0.0f;
	for (const FRogue10mTraitInvestment& Investment : Town.TraitInvestments)
	{
		if (Investment.Trait == ERogue10mTownTrait::Vitality)
		{
			VitalityRank = Investment.Rank;
			VitalityBonus = Investment.Bonus;
		}
		else if (Investment.Trait == ERogue10mTownTrait::Power)
		{
			PowerRank = Investment.Rank;
			PowerBonus = Investment.Bonus;
		}
	}
	return Town.VitalityLevel == VitalityRank && Town.PowerLevel == PowerRank
		&& Town.TraitMaxHealthBonus == VitalityBonus && Town.TraitAttackBonus == PowerBonus;
}

static bool Validate(const URogue10mProgressionSaveGame& Save, const FGuid& ProfileId)
{
	if (!ProfileId.IsValid() || Save.ProfileId != ProfileId
		|| Save.SaveVersion < 1 || Save.SaveVersion > URogue10mProgressionSaveGame::CurrentVersion
		|| Save.Growth.Level < 1 || Save.Growth.Level > 1000000)
	{
		return false;
	}
	if (Save.SaveVersion >= 4 && (Save.LobbyEarnedPoints < 0 || Save.HighestRunLevel < Save.Growth.Level
		|| Save.HighestRunLevel > 1000000 || !Save.RunId.IsValid()
		|| (Save.bRunCompleted != (Save.LastSettledRunId == Save.RunId))
		|| Save.RewardDefeatBaseline < 0
		|| Save.DiscoveredNodes.Contains(NAME_None))) { return false; }
	const auto& Town = Save.Town;
	// Old files have no canonical records. A file claiming version 1 cannot import new traits.
	if (Save.SaveVersion == 1 && !Town.TraitInvestments.IsEmpty()) { return false; }
	if (Save.SaveVersion >= 2 && !HasCanonicalTraitMirrors(Town))
	{
		return false;
	}
	if (!URogue10mTownProgressionComponent::ValidateSnapshot(Town, Save.SaveVersion >= 4 ? Save.LobbyEarnedPoints : Save.Growth.Level - 1, Save.SaveVersion >= 4 ? Save.HighestRunLevel : INDEX_NONE)) { return false; }
	if (Save.SaveVersion < 3)
	{
		// A legacy version cannot claim the new domain and bypass its ledger checks.
		if (!Save.SkillTree.Masteries.IsEmpty() || !Save.SkillTree.Investments.IsEmpty() || !Save.SkillTree.LegacyGrantedSkills.IsEmpty()
			|| Save.SkillTree.ActiveIdentity != ERogue10mSkillTreeIdentity::None) { return false; }
	}
	else if (!URogue10mSkillTreeProgressionComponent::Validate(Save.SkillTree, Save.Growth.Level - 1, Save.SaveVersion >= 4 ? 0 : TraitSpent(Town))) { return false; }
	for (const FRogue10mTraitInvestment& Investment : Town.TraitInvestments)
	{
		if (Investment.Trait == ERogue10mTownTrait::LastStand && Investment.Rank > 0 && (Save.SaveVersion >= 4 ? Save.HighestRunLevel : Save.Growth.Level) < 20)
		{
			return false;
		}
	}
	return true;
}
}

FString URogue10mProgressionSubsystem::GetSaveSlotForProfile(const FGuid& ProfileId) const
{
	return GetLegacySaveSlotForProfile(ProfileId) + TEXT("_v4");
}

FString URogue10mProgressionSubsystem::GetLegacySaveSlotForProfile(const FGuid& ProfileId) const
{
	FString Prefix = TEXT("Rogue10mProgression");
#if !UE_BUILD_SHIPPING
	FString TestPrefix;
	if (FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), TestPrefix)
		&& TestPrefix.StartsWith(TEXT("Rogue10m_Progression_QA_")) && TestPrefix.Len() <= 80)
	{
		bool bValid = true;
		for (TCHAR Character : TestPrefix) { bValid &= FChar::IsAlnum(Character) || Character == TCHAR('_'); }
		if (bValid) { Prefix = TestPrefix; }
	}
#endif
	return Prefix + TEXT("_") + ProfileId.ToString(EGuidFormats::Digits);
}

void URogue10mProgressionSubsystem::SetError(const FText& Error) const
{
	LastError = Error;
	UE_LOG(LogRogue10m, Error, TEXT("Progression save: %s"), *Error.ToString());
}

URogue10mProgressionSaveGame* URogue10mProgressionSubsystem::LoadSnapshot(
	const FGuid& ProfileId, bool& bOutExists) const
{
	FString Slot = GetSaveSlotForProfile(ProfileId);
	// An existing v4 file always wins, including an invalid one. Never silently fall back and lose progress.
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) { Slot = GetLegacySaveSlotForProfile(ProfileId); }
	bOutExists = UGameplayStatics::DoesSaveGameExist(Slot, 0);
	URogue10mProgressionSaveGame* Loaded = bOutExists
		? Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0))
		: NewObject<URogue10mProgressionSaveGame>(const_cast<URogue10mProgressionSubsystem*>(this));
	if (!Loaded)
	{
		SetError(NSLOCTEXT("Rogue10mSave", "Unreadable", "진행 저장 파일을 읽지 못했습니다. 기존 파일은 보존됩니다."));
		return nullptr;
	}
	if (!bOutExists) { Loaded->ProfileId = ProfileId; Loaded->RunId = FGuid::NewGuid(); }
	if (!Rogue10mProgressionPersistence::Validate(*Loaded, ProfileId))
	{
		SetError(NSLOCTEXT("Rogue10mSave", "Incompatible", "진행 저장 버전 또는 캐릭터 데이터가 맞지 않습니다. 기존 파일은 보존됩니다."));
		return nullptr;
	}
	if (Loaded->SaveVersion == 1)
	{
		// Keep the old version until every domain has restored successfully. No disk write here.
		Loaded->Town = URogue10mTownProgressionComponent::MigrateLegacySnapshot(Loaded->Town);
	}
	if (Loaded->SaveVersion < 3)
	{
		// Old use counters include empty swings and never represent confirmed-hit mastery.
		Loaded->SkillTree = URogue10mSkillTreeProgressionComponent::MigrateLegacy(Loaded->Combat.UnlockedSkills);
	}
	if (Loaded->SaveVersion < 4)
	{
		// Preserve prior purchased lobby upgrades, but do not convert unspent run points into lobby currency.
		Loaded->LobbyEarnedPoints = Rogue10mProgressionPersistence::TraitSpent(Loaded->Town);
		Loaded->HighestRunLevel = Loaded->Growth.Level;
		for (const auto& Pair : Loaded->Growth.MonsterDefeats) { Loaded->RewardDefeatBaseline += FMath::Max(0, Pair.Value); }
		Loaded->RunId = FGuid::NewGuid();
		for (const auto& Row : Loaded->SkillTree.Investments) { Loaded->DiscoveredNodes.Add(Row.NodeId); }
		Loaded->SaveVersion = 4;
	}
	return Loaded;
}

URogue10mProgressionSaveGame* URogue10mProgressionSubsystem::LoadSelectedSnapshot() const
{
	const URogue10mCharacterProfileSubsystem* Profiles = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const FRogue10mCharacterProfile* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (!Selected) { return nullptr; }
	if (AttachedProfileId == Selected->ProfileId && SaveData) { return SaveData; }
	if (CachedProfileId == Selected->ProfileId) { return SelectedSnapshotCache; }
	CachedProfileId = Selected->ProfileId;
	bool bExists = false;
	SelectedSnapshotCache = LoadSnapshot(CachedProfileId, bExists);
	return SelectedSnapshotCache;
}

ERogue10mDifficulty URogue10mProgressionSubsystem::GetSelectedDifficulty() const
{
	const auto* Selected = LoadSelectedSnapshot();
	if (Selected == SaveData && CurrentPlayerState.IsValid() && bRestoreValid)
	{
		return CurrentPlayerState->GetTownProgressionComponent()->GetDifficulty();
	}
	return Selected ? Selected->Town.Difficulty : ERogue10mDifficulty::Normal;
}

FRogue10mDifficultyModifiers URogue10mProgressionSubsystem::GetSelectedDifficultyModifiers() const
{
	const auto* Selected = LoadSelectedSnapshot();
	if (Selected == SaveData && CurrentPlayerState.IsValid() && bRestoreValid)
	{
		return CurrentPlayerState->GetTownProgressionComponent()->GetDifficultyModifiers();
	}
	return Selected ? Selected->Town.DifficultyModifiers : FRogue10mDifficultyModifiers();
}

bool URogue10mProgressionSubsystem::HasClaimedReward(FName RewardId) const
{
	const auto* Selected = LoadSelectedSnapshot();
	return !RewardId.IsNone() && Selected && Selected->ClaimedRewards.Contains(RewardId);
}

void URogue10mProgressionSubsystem::RecordClaimedReward(FName RewardId)
{
	if (IsSaveReady() && SaveData && !RewardId.IsNone() && !SaveData->ClaimedRewards.Contains(RewardId))
	{
		SaveData->ClaimedRewards.Add(RewardId);
		HandleStateChanged();
	}
}

bool URogue10mProgressionSubsystem::AttachCharacter(ARogue10mCharacter& Character)
{
	if (!Character.HasActorBegunPlay() || !Character.IsLocallyControlled() || !Character.HasAuthority()
		|| !Character.GetWorld() || Character.GetWorld()->GetNetMode() != NM_Standalone) { return false; }
	const URogue10mCharacterProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
	const FRogue10mCharacterProfile* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (!Selected) { return false; }
	if (CurrentCharacter.Get() == &Character && AttachedProfileId == Selected->ProfileId) { return bSaveReady; }
	if (ARogue10mCharacter* Previous = CurrentCharacter.Get()) { DetachCharacter(*Previous); }
	ARogue10mPlayerState* State = Character.GetPlayerState<ARogue10mPlayerState>();
	URogue10mInventoryComponent* Inventory = Character.GetInventoryComponent();
	URogue10mCombatComponent* Combat = Character.GetCombatComponent();
	URogue10mTownProgressionComponent* Town = State ? State->GetTownProgressionComponent() : nullptr;
	URogue10mSkillTreeProgressionComponent* Tree = State ? State->GetSkillTreeProgressionComponent() : nullptr;
	if (!State || !Inventory || !Combat || !Town || !Tree) { return false; }
	if (StarterCharacter.Get() != &Character || !PawnStarterState)
	{
		PawnStarterState = NewObject<URogue10mProgressionSaveGame>(this);
		PawnStarterState->Inventory = Inventory->CaptureInventory();
		StarterCharacter = &Character;
	}
	CurrentCharacter = &Character;
	CurrentPlayerState = State;
	AttachedProfileId = Selected->ProfileId;
	bSaveReady = false;
	bRestoreValid = false;
	bDirty = false;
	bool bExists = false;
	SaveData = LoadSnapshot(AttachedProfileId, bExists);
	if (!SaveData) { return false; }
	const bool bNeedsMigration = bExists && !UGameplayStatics::DoesSaveGameExist(GetSaveSlotForProfile(AttachedProfileId), 0);
	const bool bResetCompletedRun = SaveData->bRunCompleted;
	const int32 PreviousLobbyBudget = Town->GetLobbyEarnedBudget();
	const int32 PreviousUnlockLevel = Town->GetLobbyUnlockLevel();
	const auto OriginalSaveData = TStrongObjectPtr<URogue10mProgressionSaveGame>(DuplicateObject(SaveData.Get(), this));
	Town->SetLobbyBudget(SaveData->LobbyEarnedPoints, SaveData->HighestRunLevel);
	if (bResetCompletedRun)
	{
		// Use this fresh pawn's authored starter kit, never the completed run's equipment or learned skills.
		SaveData->Growth = FRogue10mGrowthSnapshot();
		SaveData->Growth.WeaponType = Character.GetEquippedWeaponType();
		SaveData->Inventory = PawnStarterState->Inventory;
		SaveData->Combat = FRogue10mCombatSnapshot();
		SaveData->SkillTree = FRogue10mSkillTreeSnapshot();
		SaveData->ClaimedRewards.Reset(); // NPC receipts are run-scoped; Discovery has its own permanent domain.
		SaveData->RunId = FGuid::NewGuid();
		SaveData->RewardDefeatBaseline = 0;
		SaveData->bRunCompleted = false;
	}
	{
	TGuardValue<bool> RestoreGuard(bRestoring, true);
	if (bExists)
	{
		const auto PreviousGrowth = State->CaptureGrowth();
		const auto PreviousInventory = Inventory->CaptureInventory();
		const auto PreviousCombat = Combat->CaptureCombat();
		const auto PreviousTown = Town->CaptureProgression();
		const auto PreviousTree = Tree->Capture();
		const TCHAR* FailedStage = nullptr;
		if (!State->RestoreGrowth(SaveData->Growth)) { FailedStage = TEXT("Growth"); }
		else if (!Inventory->RestoreInventory(SaveData->Inventory)) { FailedStage = TEXT("Inventory"); }
		else if (!Tree->RestoreForTransaction(SaveData->SkillTree, false, 0)) { FailedStage = TEXT("SkillTree"); }
		else if (!Town->RestoreProgression(SaveData->Town)) { FailedStage = TEXT("Town"); }
		else if (bResetCompletedRun)
		{
			Tree->RefreshAutomaticUnlocks();
			Combat->ResetCombatProgression();
			SaveData->Combat = Combat->CaptureCombat();
		}
		else if (!Combat->RestoreCombat(SaveData->Combat)) { FailedStage = TEXT("Combat"); }
		if (FailedStage)
		{
			SaveData = OriginalSaveData.Get();
			Town->SetLobbyBudget(PreviousLobbyBudget, PreviousUnlockLevel);
			const bool bGrowthRolledBack = State->RestoreGrowth(PreviousGrowth);
			const bool bInventoryRolledBack = Inventory->RestoreInventory(PreviousInventory);
			const bool bTreeRolledBack = Tree->RestoreForTransaction(PreviousTree, false, PreviousLobbyBudget >= 0 ? 0 : Rogue10mProgressionPersistence::TraitSpent(PreviousTown));
			const bool bTownRolledBack = Town->RestoreProgression(PreviousTown);
			const bool bCombatRolledBack = Combat->RestoreCombat(PreviousCombat);
			UE_LOG(LogRogue10m, Error, TEXT("Progression restore rejected profile=%s stage=%s rollback_growth=%d rollback_inventory=%d rollback_tree=%d rollback_town=%d rollback_combat=%d"),
				*AttachedProfileId.ToString(), FailedStage, bGrowthRolledBack, bInventoryRolledBack, bTreeRolledBack, bTownRolledBack, bCombatRolledBack);
			SetError(NSLOCTEXT("Rogue10mSave", "RestoreFailed", "진행 데이터를 복원하지 못했습니다. 누락 에셋과 인벤토리 배치를 확인해 주세요. 기존 파일은 보존됩니다."));
			return false;
		}
	}
	else
	{
		// A reused Controller owns its old PlayerState. New profiles must start with fresh growth.
		FRogue10mGrowthSnapshot NewGrowth;
		NewGrowth.WeaponType = Character.GetEquippedWeaponType();
		State->RestoreGrowth(NewGrowth);
		Tree->RestoreForTransaction(FRogue10mSkillTreeSnapshot(), false, 0);
		Tree->RefreshAutomaticUnlocks();
		Town->RestoreProgression(FRogue10mTownProgressionSnapshot());
		Combat->ResetCombatProgression();
	}
	Tree->RefreshAutomaticUnlocks();
	Character.RefreshCharacterStats(true);
	State->SetCharacterDead(false);
	bSaveReady = true;
	bRestoreValid = true;
	LastError = FText::GetEmpty();
	BindChanges(Character);
	} // Restoration is complete before the persistence gate is evaluated.
	if (!bExists || bNeedsMigration || bResetCompletedRun)
	{
		bDirty = true;
		CaptureCurrentState();
		SaveNow();
	}
	UE_LOG(LogRogue10m, Log, TEXT("Progression attached profile=%s restored=%d"),
		*AttachedProfileId.ToString(), bExists);
	return bSaveReady;
}

bool URogue10mProgressionSubsystem::IsAttachedTo(const ARogue10mCharacter& Character) const
{
	return CurrentCharacter.Get() == &Character;
}

bool URogue10mProgressionSubsystem::IsSaveReady() const
{
	const auto* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	return bSaveReady && CurrentCharacter.IsValid() && Selected && Selected->IsValid()
		&& Selected->ProfileId == AttachedProfileId;
}

bool URogue10mProgressionSubsystem::CanRetrySave() const
{
	const UGameInstance* Instance = GetGameInstance();
	const auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	const ARogue10mCharacter* Character = CurrentCharacter.Get();
	const ARogue10mPlayerState* State = CurrentPlayerState.Get();
	return bRestoreValid && !bRestoring && SaveData && Character && State
		&& Character->HasAuthority() && Character->IsLocallyControlled() && Character->GetWorld()
		&& Character->GetWorld()->GetNetMode() == NM_Standalone
		&& Character->GetPlayerState<ARogue10mPlayerState>() == State && State->GetPawn() == Character
		&& AttachedProfileId.IsValid() && Selected && Selected->IsValid()
		&& Selected->ProfileId == AttachedProfileId && SaveData->ProfileId == AttachedProfileId;
}

void URogue10mProgressionSubsystem::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	if (Phase == ERogue10mRunPhase::Running && SaveData && CurrentCharacter.IsValid())
	{
		SettlementProfileId = AttachedProfileId;
		SettlementRunId = SaveData->RunId;
	}
}

void URogue10mProgressionSubsystem::BindChanges(ARogue10mCharacter& Character)
{
	ObservedRun = Character.GetWorld()->GetGameState<ARogue10mGameState>();
	if (ObservedRun.IsValid())
	{
		ObservedRun->OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhaseChanged);
		// Travel may start the timer before the pawn attaches; terminal worlds never adopt a new owner.
		HandleRunPhaseChanged(ObservedRun->GetRunPhase());
	}
	Character.GetInventoryComponent()->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
	Character.GetCombatComponent()->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
	if (ARogue10mPlayerState* State = CurrentPlayerState.Get())
	{
		State->OnGrowthChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
		State->OnMartialArtsProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
		State->GetTownProgressionComponent()->OnTownProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
		State->GetSkillTreeProgressionComponent()->OnChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
	}
}

void URogue10mProgressionSubsystem::UnbindChanges()
{
	if (ObservedRun.IsValid()) { ObservedRun->OnRunPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRunPhaseChanged); }
	ObservedRun.Reset();
	SettlementProfileId.Invalidate();
	SettlementRunId.Invalidate();
	if (ARogue10mCharacter* Character = CurrentCharacter.Get())
	{
		Character->GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimer);
		Character->GetInventoryComponent()->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
		Character->GetCombatComponent()->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
	}
	if (ARogue10mPlayerState* State = CurrentPlayerState.Get())
	{
		State->OnGrowthChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
		State->OnMartialArtsProgressChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
		State->GetTownProgressionComponent()->OnTownProgressionChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
		State->GetSkillTreeProgressionComponent()->OnChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
	}
}

void URogue10mProgressionSubsystem::HandleStateChanged()
{
	if (!bRestoreValid || bRestoring || !CurrentCharacter.IsValid()) { return; }
	bDirty = true;
	FTimerManager& Timers = CurrentCharacter->GetWorld()->GetTimerManager();
	if (!Timers.IsTimerActive(AutoSaveTimer))
	{
		Timers.SetTimer(AutoSaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveNow(); }), 1.0f, false);
	}
}

void URogue10mProgressionSubsystem::CaptureCurrentState()
{
	ARogue10mCharacter* Character = CurrentCharacter.Get();
	ARogue10mPlayerState* State = CurrentPlayerState.Get();
	if (!SaveData || !Character || !State) { return; }
	SaveData->ProfileId = AttachedProfileId;
	SaveData->Growth = State->CaptureGrowth();
	SaveData->Inventory = Character->GetInventoryComponent()->CaptureInventory();
	SaveData->Combat = Character->GetCombatComponent()->CaptureCombat();
	SaveData->Town = State->GetTownProgressionComponent()->CaptureProgression();
	SaveData->SkillTree = State->GetSkillTreeProgressionComponent()->Capture();
	SaveData->HighestRunLevel = FMath::Max(SaveData->HighestRunLevel, SaveData->Growth.Level);
	for (const auto& Row : SaveData->SkillTree.Investments) { SaveData->DiscoveredNodes.Add(Row.NodeId); }
	State->GetTownProgressionComponent()->SetLobbyBudget(SaveData->LobbyEarnedPoints, SaveData->HighestRunLevel);
	SaveData->SaveVersion = URogue10mProgressionSaveGame::CurrentVersion;
}

int32 URogue10mProgressionSubsystem::GetLobbyAvailablePoints() const
{
	const auto* Snapshot = LoadSelectedSnapshot();
	if (Snapshot == SaveData && bRestoreValid && CurrentPlayerState.IsValid())
	{
		return CurrentPlayerState->GetTownProgressionComponent()->GetAvailableTraitPoints();
	}
	return Snapshot ? FMath::Max(0, Snapshot->LobbyEarnedPoints - Rogue10mProgressionPersistence::TraitSpent(Snapshot->Town)) : 0;
}

bool URogue10mProgressionSubsystem::HasDiscoveredNode(FName NodeId) const
{
	const auto* Snapshot = LoadSelectedSnapshot();
	return !NodeId.IsNone() && Snapshot && Snapshot->DiscoveredNodes.Contains(NodeId);
}

bool URogue10mProgressionSubsystem::SaveRunSettlement(bool bVictory)
{
	if (!CanRetrySave()) { return false; }
	const auto* Run = CurrentCharacter->GetWorld()->GetGameState<ARogue10mGameState>();
	if (!Run || Run != ObservedRun.Get() || SettlementProfileId != AttachedProfileId || SettlementRunId != SaveData->RunId
		|| (bVictory ? Run->GetRunPhase() != ERogue10mRunPhase::Victory : Run->GetRunPhase() != ERogue10mRunPhase::Defeat)) { return false; }
	CaptureCurrentState();
	if (!Rogue10mProgressionPersistence::Validate(*SaveData, AttachedProfileId)) { return false; }
	if (SaveData->LastSettledRunId != SaveData->RunId)
	{
		int64 Defeats = 0;
		for (const auto& Pair : SaveData->Growth.MonsterDefeats) { Defeats += FMath::Max(0, Pair.Value); }
		const int64 Reward = FMath::Max<int64>(0, Defeats - SaveData->RewardDefeatBaseline) * FMath::Clamp(LobbyPointsPerDefeat, 0, 1000)
			+ (bVictory ? FMath::Clamp(LobbyVictoryBonus, 0, 10000) : 0);
		SaveData->LobbyEarnedPoints = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(SaveData->LobbyEarnedPoints) + Reward));
		SaveData->LastSettledRunId = SaveData->RunId;
		SaveData->bRunCompleted = true;
	}
	return SaveNow();
}

bool URogue10mProgressionSubsystem::SaveNow()
{
	// Selection changes its destination before GameMode saves the previous attached pawn.
	// This write stays bound to the previous pawn's own profile; settlement requires selection too.
	const auto* Character = CurrentCharacter.Get();
	const auto* Player = CurrentPlayerState.Get();
	if (!bRestoreValid || bRestoring || !SaveData || !Character || !Player || !AttachedProfileId.IsValid()
		|| SaveData->ProfileId != AttachedProfileId || !Character->HasAuthority() || !Character->IsLocallyControlled()
		|| !Character->GetWorld() || Character->GetWorld()->GetNetMode() != NM_Standalone
		|| Character->GetPlayerState<ARogue10mPlayerState>() != Player || Player->GetPawn() != Character) { return false; }
	CurrentCharacter->GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimer);
	CaptureCurrentState();
	if (!Rogue10mProgressionPersistence::Validate(*SaveData, AttachedProfileId))
	{
		bDirty = true;
		bSaveReady = false;
		SetError(NSLOCTEXT("Rogue10mSave", "InvalidCapture", "특성 투자 데이터가 올바르지 않아 저장하지 못했습니다. 기존 파일은 보존됩니다."));
		return false;
	}
	if (!UGameplayStatics::SaveGameToSlot(SaveData, GetSaveSlotForProfile(AttachedProfileId), 0))
	{
		bDirty = true;
		bSaveReady = false;
		SetError(NSLOCTEXT("Rogue10mSave", "WriteFailed", "성장과 인벤토리를 저장하지 못했습니다. 저장 공간과 권한을 확인해 주세요."));
		return false;
	}
	bDirty = false;
	bSaveReady = true;
	LastError = FText::GetEmpty();
	CachedProfileId = AttachedProfileId;
	SelectedSnapshotCache = SaveData;
	return true;
}

void URogue10mProgressionSubsystem::DetachCharacter(ARogue10mCharacter& Character, bool bSave)
{
	if (!IsAttachedTo(Character)) { return; }
	if (bSave && bRestoreValid) { SaveNow(); }
	UnbindChanges();
	CurrentCharacter.Reset();
	CurrentPlayerState.Reset();
	AttachedProfileId.Invalidate();
	SaveData = nullptr;
	bSaveReady = false;
	bRestoreValid = false;
	bDirty = false;
}

void URogue10mProgressionSubsystem::Deinitialize()
{
	if (ARogue10mCharacter* Character = CurrentCharacter.Get()) { DetachCharacter(*Character); }
	Super::Deinitialize();
}
