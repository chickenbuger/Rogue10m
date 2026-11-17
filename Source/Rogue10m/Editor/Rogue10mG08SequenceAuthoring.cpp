// G08 prototype authoring only. No runtime activation, profile edits or source asset saves.
#if WITH_EDITOR
#include <limits>
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAuthoredContactTimeline.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace Rogue10mG08SequenceAuthoring
{
static const TCHAR* Root = TEXT("/Game/Rogue10m/Skills/MeleeSequence20261113/G08");
static const TCHAR* Schema = TEXT("MeleeSequence20261113.G08.v2");
static const TCHAR* AnimationRoot = TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110");
static const TCHAR* SourceDataPath = TEXT("/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Primary01");
static const TCHAR* SourceNames[] = {TEXT("A_GreatSword_Primary01"), TEXT("A_GreatSword_Primary02"), TEXT("A_GreatSword_Primary03")};
static const TCHAR* DeclaredFiles[] = {TEXT("great sword attack.fbx"), TEXT("great sword slash.fbx"), TEXT("great sword slash (3).fbx")};
static const TCHAR* NewNames[] = {TEXT("A_G08_Attack_Prototype"), TEXT("A_G08_Slash_Prototype"), TEXT("A_G08_Slash03_Prototype"),
	TEXT("AM_G08_AnnihilationChain_Prototype"), TEXT("DA_G08_AnnihilationChain_Prototype")};
static const TCHAR* Pending = TEXT("Source-local contacts and montage-local recovery are authored inputs, not visually verified. Explicit zero recovery retains V1 contact-only behavior. Three frontal Arc cuts; one hit per target per contact. Arc range/angle/target cap are temporary source carryovers. Full source clips, final trims, joins, two-hand trajectories, actual third-person arc geometry/contact/recovery, VFX and balance remain pending. No production opt-in.");

struct FOptions
{
	bool bApply = false;
	bool bContactsSupplied = false;
	bool bRateSupplied = false;
	bool bRecoverySupplied = false;
	TArray<double> Contacts;
	double SkillRate = 1.0;
	double RecoveryEndTime = 0.0;
};

struct FSourceStamp
{
	UObject* Object = nullptr;
	FString Filename;
	FString SHA1;
	bool bWasDirty = false;
};

static TArray<TSharedPtr<FJsonValue>> Numbers(const TArray<double>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (double Value : Values) { Result.Add(MakeShared<FJsonValueNumber>(Value)); }
	return Result;
}

static bool ValidArcGeometry(double Range, double AngleDegrees)
{
	return FMath::IsFinite(Range) && Range > 0.0 && FMath::IsFinite(AngleDegrees) && AngleDegrees > 0.0 && AngleDegrees <= 180.0;
}

static bool ValidRecovery(double Recovery, double LastContact, double Rate, double Length)
{
	return FMath::IsFinite(Recovery) && Recovery >= 0.0 && (Recovery == 0.0
		|| (FMath::IsFinite(LastContact) && Recovery >= LastContact && FMath::IsFinite(Length) && Recovery <= Length
			&& FMath::IsFinite(Rate) && Rate > 0.0 && Recovery / Rate <= 30.0));
}

static bool Parse(const TArray<FString>& Args, FOptions& Options, FString& Error)
{
	int32 Index = 0;
	if (Args.Num() && (Args[0] == TEXT("plan") || Args[0] == TEXT("apply")))
	{ Options.bApply = Args[0] == TEXT("apply"); Index = 1; }
	for (; Index < Args.Num(); ++Index)
	{
		FString Key, Value;
		if (!Args[Index].Split(TEXT("="), &Key, &Value)) { Error = TEXT("Expected plan/apply, contacts=a,b,c, rate=positive_value and recovery=montage_1x_end_or_zero"); return false; }
		if (Key == TEXT("contacts") && !Options.bContactsSupplied)
		{
			Options.bContactsSupplied = true;
			TArray<FString> Parts; Value.ParseIntoArray(Parts, TEXT(","), false);
			if (Parts.Num() != 3) { Error = TEXT("Exactly three explicit source-local contacts are required"); return false; }
			for (const FString& Part : Parts)
			{
				double Number;
				if (!FDefaultValueHelper::ParseDouble(Part, Number) || !FMath::IsFinite(Number) || Number < 0.0)
				{ Error = TEXT("Contacts must be finite nonnegative source-local seconds"); return false; }
				Options.Contacts.Add(Number);
			}
		}
		else if (Key == TEXT("rate") && !Options.bRateSupplied)
		{
			Options.bRateSupplied = true;
			if (!FDefaultValueHelper::ParseDouble(Value, Options.SkillRate) || !FMath::IsFinite(Options.SkillRate)
				|| Options.SkillRate <= 0.0 || Options.SkillRate > MAX_flt || FMath::IsNearlyZero(static_cast<float>(Options.SkillRate)))
			{ Error = TEXT("An explicit finite positive nonzero float skill rate is required"); return false; }
		}
		else if (Key == TEXT("recovery") && !Options.bRecoverySupplied)
		{
			Options.bRecoverySupplied = true;
			if (!FDefaultValueHelper::ParseDouble(Value, Options.RecoveryEndTime) || !FMath::IsFinite(Options.RecoveryEndTime)
				|| Options.RecoveryEndTime < 0.0 || Options.RecoveryEndTime > MAX_flt
				|| (Options.RecoveryEndTime > 0.0 && static_cast<float>(Options.RecoveryEndTime) == 0.0f))
			{ Error = TEXT("Recovery requires finite nonnegative float montage-1x absolute seconds; explicit zero retains V1 contact-only behavior"); return false; }
		}
		else { Error = TEXT("Unknown or duplicate option: ") + Key; return false; }
	}
	if (Options.bApply && (!Options.bContactsSupplied || !Options.bRateSupplied || !Options.bRecoverySupplied))
	{ Error = TEXT("Apply requires contacts=a,b,c, rate=... and recovery=... (explicit zero is supported); no timing guesses are made"); return false; }
	return true;
}

static FString PackageFilename(const FString& PackageName)
{
	return FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
}

static bool HashFile(const FString& Filename, FString& Result)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename)) { return false; }
	FSHAHash Hash; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash.Hash);
	Result = Hash.ToString(); return true;
}

static bool Capture(UObject* Object, FSourceStamp& Stamp, FString& Error)
{
	Stamp.Object = Object; Stamp.bWasDirty = Object->GetOutermost()->IsDirty();
	if (Stamp.bWasDirty) { Error = TEXT("Unsaved/dirty source cannot be fingerprinted as the disk package; owner must resolve outside this command: ") + Object->GetPathName(); return false; }
	Stamp.Filename = PackageFilename(Object->GetOutermost()->GetName());
	if (!HashFile(Stamp.Filename, Stamp.SHA1)) { Error = TEXT("Cannot fingerprint local source package: ") + Stamp.Filename; return false; }
	return true;
}

static bool SourcesUnchanged(const TArray<FSourceStamp>& Stamps, FString& Error)
{
	for (const auto& Stamp : Stamps)
	{
		FString Now;
		if (!HashFile(Stamp.Filename, Now) || Now != Stamp.SHA1 || Stamp.Object->GetOutermost()->IsDirty() != Stamp.bWasDirty)
		{ Error = TEXT("Source package changed during authoring: ") + Stamp.Object->GetPathName(); return false; }
	}
	return true;
}

static bool Fresh(const FString& PackageName, FString& Error)
{
	if (!PackageName.StartsWith(FString(Root) + TEXT("/")) || !FPackageName::IsValidLongPackageName(PackageName)
		|| FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName)
		|| IFileManager::Get().FileExists(*PackageFilename(PackageName)))
	{ Error = TEXT("Destination must be fresh; existing/loaded packages are never reused or overwritten: ") + PackageName; return false; }
	return true;
}

static FArrayProperty* ContactProperty(FString& Error)
{
	auto* Property = FindFProperty<FArrayProperty>(URogue10mAttackSkillData::StaticClass(), TEXT("AuthoredContactTimes"));
	if (!Property || !CastField<FFloatProperty>(Property->Inner))
	{ Error = TEXT("Stale/unsupported reflection: AuthoredContactTimes must be TArray<float>; rebuild/restart under the editor owner"); return nullptr; }
	return Property;
}

static FFloatProperty* RecoveryProperty(FString& Error)
{
	auto* Property = FindFProperty<FFloatProperty>(URogue10mAttackSkillData::StaticClass(), TEXT("AuthoredRecoveryEndTime"));
	if (!Property)
	{ Error = TEXT("Stale/unsupported reflection: AuthoredRecoveryEndTime must be float; rebuild/restart under the editor owner"); return nullptr; }
	return Property;
}

static void SetContacts(URogue10mAttackSkillData& Skill, FArrayProperty& Property, const TArray<double>& Contacts)
{
	FScriptArrayHelper Array(&Property, Property.ContainerPtrToValuePtr<void>(&Skill));
	Array.Resize(Contacts.Num());
	auto* Float = CastFieldChecked<FFloatProperty>(Property.Inner);
	for (int32 Index = 0; Index < Contacts.Num(); ++Index) { Float->SetPropertyValue(Array.GetRawPtr(Index), static_cast<float>(Contacts[Index])); }
}

static bool CheckContacts(const URogue10mAttackSkillData& Skill, FArrayProperty& Property, const TArray<double>& Contacts,
	double Length, double RecoveryEnd, FString& Error)
{
	FScriptArrayHelper Array(&Property, Property.ContainerPtrToValuePtr<void>(&Skill));
	auto* Float = CastFieldChecked<FFloatProperty>(Property.Inner);
	if (Array.Num() != 3 || Skill.HitCount != 3) { Error = TEXT("Contact count/readback mismatch"); return false; }
	TArray<float> RuntimeTimes;
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		const double Value = Float->GetPropertyValue(Array.GetRawPtr(Index));
		if (!FMath::IsNearlyEqual(Value, Contacts[Index], 0.00001)) { Error = TEXT("Contact float readback mismatch"); return false; }
		RuntimeTimes.Add(static_cast<float>(Value));
	}
	if (!FRogue10mAuthoredContactTimeline::Validate(RuntimeTimes.GetData(), RuntimeTimes.Num(), Skill.AnimationPlayRate, Length, 0.0, RecoveryEnd))
	{ Error = TEXT("Exact runtime contact/recovery kernel rejected the authored float readback"); return false; }
	return true;
}

static bool CheckRecovery(const URogue10mAttackSkillData& Skill, FFloatProperty& Property, double Expected,
	const TArray<double>& Contacts, double Length, FString& Error)
{
	const float Value = Property.GetPropertyValue_InContainer(&Skill);
	if (Value != static_cast<float>(Expected) || Contacts.IsEmpty()
		|| !ValidRecovery(Value, static_cast<float>(Contacts.Last()), Skill.AnimationPlayRate, Length))
	{ Error = TEXT("Recovery float readback/order/montage-length/30-second limit mismatch"); return false; }
	return true;
}

static bool ClipCopyEqual(UAnimSequence& Source, UAnimSequence& Copy, FString& Error)
{
	auto* A = Source.GetDataModel(); auto* B = Copy.GetDataModel();
	auto* CopyModelObject = Copy.GetDataModelInterface().GetObject();
	if (!A || !B || Copy.GetSkeleton() != Source.GetSkeleton() || Copy.HasParentAsset() || Copy.HasRootMotion()
		|| A == B || !CopyModelObject || CopyModelObject->GetTypedOuter<UAnimSequence>() != &Copy
		|| !Copy.Notifies.IsEmpty() || Copy.RateScale != Source.RateScale || A->GetFrameRate() != B->GetFrameRate()
		|| A->GetNumberOfKeys() != B->GetNumberOfKeys() || A->GetNumberOfFrames() != B->GetNumberOfFrames()
		|| A->GetNumberOfKeys() < 2 || A->GetNumberOfKeys() > 10000 || !FMath::IsNearlyEqual(A->GetPlayLength(), B->GetPlayLength(), 0.00000001))
	{ Error = TEXT("Duplicated animation skeleton/rate/root/notify/model contract mismatch"); return false; }
	TArray<FName> NamesA, NamesB; A->GetBoneTrackNames(NamesA); B->GetBoneTrackNames(NamesB);
	if (NamesA.IsEmpty() || NamesA != NamesB) { Error = TEXT("Duplicated bone-track order mismatch"); return false; }
	for (FName Bone : NamesA)
	{
		TArray<FTransform> KeysA, KeysB; A->GetBoneTrackTransforms(Bone, KeysA); B->GetBoneTrackTransforms(Bone, KeysB);
		if (KeysA.Num() != A->GetNumberOfKeys() || KeysA.Num() != KeysB.Num()) { Error = TEXT("Duplicated key count mismatch"); return false; }
		for (int32 Index = 0; Index < KeysA.Num(); ++Index)
		{
			if (KeysA[Index].ContainsNaN() || KeysB[Index].ContainsNaN() || !KeysA[Index].Equals(KeysB[Index], 0.000001))
			{ Error = TEXT("Duplicated native transform keys changed: ") + Bone.ToString(); return false; }
		}
	}
	return true;
}

static bool ProtectedDataEqual(const URogue10mAttackSkillData& Source, const URogue10mAttackSkillData& Copy, FString& Error)
{
	static const TSet<FName> Overrides = {TEXT("SkillName"), TEXT("SkillDescription"), TEXT("AllowedSlotPolicy"), TEXT("InputSlot"),
		TEXT("SkillTreeNodeType"), TEXT("HitMode"), TEXT("HitCount"), TEXT("HitStartDelaySeconds"), TEXT("AuthoredContactTimes"), TEXT("AuthoredRecoveryEndTime"),
		TEXT("bEnableCombo"), TEXT("NextComboSkill"), TEXT("bAdvanceBeforeHit"), TEXT("AttackMontage"), TEXT("AnimationPlayRate"), TEXT("AttackShape")};
	for (TFieldIterator<FProperty> It(Source.GetClass()); It; ++It)
	{
		if (!Overrides.Contains(It->GetFName()) && !It->Identical_InContainer(&Source, &Copy))
		{ Error = TEXT("Protected copied skill property changed: ") + It->GetName(); return false; }
	}
	return true;
}

static void Metadata(UObject& Object, const FString& Source, const FString& SHA1)
{
	auto& Meta = Object.GetOutermost()->GetMetaData();
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.Schema"), Schema);
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.Source"), *Source);
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.SourcePackageSHA1"), *SHA1);
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.Pending"), Pending);
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.TimingVerified"), TEXT("false"));
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.RecoveryVerified"), TEXT("false"));
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.ThirdPersonVerified"), TEXT("false"));
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.ProductionOptIn"), TEXT("false"));
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.AccountOwnershipVerified"), TEXT("false"));
	Meta.SetValue(&Object, TEXT("Rogue10m.G08.GeometryVerified"), TEXT("false"));
}

static bool SaveFresh(UObject& Object, FString& Error)
{
	auto* Package = Object.GetOutermost(); const FString Filename = PackageFilename(Package->GetName());
	// Repeat the disk check immediately before saving. No retry/overwrite path exists.
	if (IFileManager::Get().FileExists(*Filename)) { Error = TEXT("Destination appeared before save; refusing overwrite: ") + Filename; return false; }
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)) { Error = TEXT("Cannot create destination directory"); return false; }
	FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone; SaveArgs.SaveFlags = SAVE_None;
	if (!UPackage::SavePackage(Package, &Object, *Filename, SaveArgs)) { Error = TEXT("SavePackage failed: ") + Filename; return false; }
	return true;
}

static bool Author(const FOptions& Options, const TSharedRef<FJsonObject>& Report, FString& Error)
{
	Report->SetStringField(TEXT("destination_prefix"), Root);
	Report->SetStringField(TEXT("schema"), Schema);
	Report->SetBoolField(TEXT("apply_requested"), Options.bApply);
	Report->SetBoolField(TEXT("timing_authored"), Options.bContactsSupplied);
	Report->SetBoolField(TEXT("timing_verified"), false);
	Report->SetBoolField(TEXT("recovery_explicit"), Options.bRecoverySupplied);
	Report->SetBoolField(TEXT("recovery_verified"), false);
	Report->SetBoolField(TEXT("third_person_verified"), false);
	Report->SetBoolField(TEXT("account_ownership_verified"), false);
	Report->SetBoolField(TEXT("production_opt_in"), false);
	Report->SetStringField(TEXT("pending"), Pending);
	Report->SetStringField(TEXT("clip_range_policy"), TEXT("Full source clips; no legacy montage trim/contact inference"));
	Report->SetStringField(TEXT("input_contact_units"), TEXT("Three full-source local clip offsets in seconds; explicit author inputs, not approval"));
	Report->SetBoolField(TEXT("skill_rate_explicit_temporary"), Options.bRateSupplied);
	if (Options.bRateSupplied) { Report->SetNumberField(TEXT("authored_skill_rate"), Options.SkillRate); }
	Report->SetStringField(TEXT("input_recovery_units"), TEXT("Absolute montage seconds at 1x; zero explicitly retains V1 contact-only behavior, independent of AttackCooldown"));
	if (Options.bRecoverySupplied)
	{
		Report->SetNumberField(TEXT("authored_recovery_end_time_1x"), Options.RecoveryEndTime);
		Report->SetStringField(TEXT("recovery_mode"), Options.RecoveryEndTime == 0.0 ? TEXT("v1_contact_only") : TEXT("explicit_montage_recovery_end"));
		if (Options.bRateSupplied) { Report->SetNumberField(TEXT("authored_recovery_end_time_at_skill_rate"), Options.RecoveryEndTime / Options.SkillRate); }
	}
	Report->SetBoolField(TEXT("planned_montage_auto_blend_out"), true);
	Report->SetNumberField(TEXT("planned_montage_blend_out_trigger_time"), 0.0);
	auto* Contact = ContactProperty(Error);
	auto* Recovery = RecoveryProperty(Error);
	if (!Contact || !Recovery) { return false; }
	// Property/registry/save callbacks can invoke GC. Keep sources, new packages
	// and every new asset alive across those external editor callback boundaries.
	TArray<TStrongObjectPtr<UObject>> KeepAlive;
	TArray<FString> Packages;
	for (const TCHAR* Name : NewNames)
	{
		const FString Package = FString(Root) + TEXT("/") + Name;
		if (!Fresh(Package, Error)) { return false; }
		Packages.Add(Package);
	}
	TArray<TSharedPtr<FJsonValue>> Destinations;
	for (const auto& Package : Packages) { Destinations.Add(MakeShared<FJsonValueString>(Package)); }
	Report->SetArrayField(TEXT("planned_packages"), Destinations);
	auto* SourceData = LoadObject<URogue10mAttackSkillData>(nullptr, SourceDataPath);
	if (!SourceData || SourceData->GetClass() != URogue10mAttackSkillData::StaticClass()
		|| SourceData->AttackShape == ERogue10mAttackShape::Projectile)
	{ Error = TEXT("Exact native non-projectile source skill is missing/unsupported"); return false; }
	KeepAlive.Emplace(SourceData);
	if (!ValidArcGeometry(SourceData->AttackRange, SourceData->ArcAngleDegrees))
	{ Error = TEXT("Temporary source Arc geometry must have finite range > 0 and finite 0 < angle <= 180 degrees"); return false; }
	Report->SetStringField(TEXT("source_shape"), StaticEnum<ERogue10mAttackShape>()->GetNameStringByValue(static_cast<int64>(SourceData->AttackShape)));
	Report->SetStringField(TEXT("candidate_shape"), TEXT("Arc"));
	Report->SetNumberField(TEXT("source_range"), SourceData->AttackRange);
	Report->SetNumberField(TEXT("candidate_range"), SourceData->AttackRange);
	Report->SetNumberField(TEXT("source_arc_angle_degrees"), SourceData->ArcAngleDegrees);
	Report->SetNumberField(TEXT("candidate_arc_angle_degrees"), SourceData->ArcAngleDegrees);
	Report->SetNumberField(TEXT("candidate_effective_runtime_arc_angle_degrees"), FMath::Clamp(SourceData->ArcAngleDegrees, 1.0f, 180.0f));
	Report->SetNumberField(TEXT("source_max_targets_per_hit"), SourceData->MaxTargetsPerHit);
	Report->SetNumberField(TEXT("candidate_max_targets_per_hit"), SourceData->MaxTargetsPerHit);
	Report->SetNumberField(TEXT("source_damage"), SourceData->Damage);
	Report->SetNumberField(TEXT("candidate_damage"), SourceData->Damage);
	Report->SetNumberField(TEXT("source_attack_cooldown"), SourceData->AttackCooldown);
	Report->SetNumberField(TEXT("candidate_attack_cooldown"), SourceData->AttackCooldown);
	Report->SetBoolField(TEXT("geometry_verified"), false);
	Report->SetStringField(TEXT("geometry_tuning_status"), TEXT("Source range/angle/MaxTargets carried over temporarily; actual 3P arc geometry and final balance unapproved"));
	Report->SetStringField(TEXT("damage_cost_cooldown_vfx_policy"), TEXT("Unchanged from source; complete reflected protected-field equality is required before apply saves"));
	Report->SetBoolField(TEXT("damage_cost_cooldown_vfx_verified_unchanged"), false);
	TArray<UAnimSequence*> Sources;
	TArray<FSourceStamp> Stamps;
	TArray<FAnimSegment> PlanSegments;
	TArray<double> Contacts;
	TArray<TSharedPtr<FJsonValue>> SourceRows;
	USkeleton* Skeleton = nullptr; double Cursor = 0.0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FString Path = FString(AnimationRoot) + TEXT("/") + SourceNames[Index];
		auto* Clip = LoadObject<UAnimSequence>(nullptr, *Path);
		if (!Clip || !Clip->GetSkeleton() || !Clip->GetDataModel() || Clip->GetDataModel()->GetNumberOfKeys() < 2
			|| Clip->HasParentAsset() || Clip->HasRootMotion() || !Clip->Notifies.IsEmpty()
			|| !FMath::IsFinite(Clip->GetPlayLength()) || Clip->GetPlayLength() <= 0.0
			|| !FMath::IsFinite(Clip->RateScale) || Clip->RateScale <= 0.0f || FMath::IsNearlyZero(Clip->RateScale))
		{ Error = TEXT("Source requires existing non-parented, non-root-motion, notify-free, finite forward-playback sequence: ") + Path; return false; }
		if (Skeleton && Clip->GetSkeleton() != Skeleton) { Error = TEXT("Three source skeletons must match exactly"); return false; }
		Skeleton = Clip->GetSkeleton(); Sources.Add(Clip);
		KeepAlive.Emplace(Clip);
		FSourceStamp Stamp;
		if (!Capture(Clip, Stamp, Error)) { return false; }
		Stamps.Add(Stamp);
		FAnimSegment Segment; Segment.SetAnimReference(Clip, true);
		Segment.StartPos = static_cast<float>(Cursor); Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = Clip->GetPlayLength(); Segment.AnimPlayRate = 1.0f; Segment.LoopingCount = 1;
		const double EffectiveRate = Segment.GetValidPlayRate(); const double Length = Segment.GetLength();
		if (!FMath::IsFinite(EffectiveRate) || EffectiveRate <= 0.0 || !FMath::IsFinite(Length) || Length <= 0.0)
		{ Error = TEXT("Invalid effective source rate or segment length"); return false; }
		if (Options.bContactsSupplied)
		{
			if (Options.Contacts[Index] >= Clip->GetPlayLength()) { Error = TEXT("Contact must be before its exact source clip end: ") + Path; return false; }
			const double Time = Segment.StartPos + Options.Contacts[Index] / EffectiveRate;
			if (!FMath::IsFinite(Time) || (!Contacts.IsEmpty() && Time <= Contacts.Last())
				|| (Options.bRateSupplied && Time / Options.SkillRate > 30.0))
			{ Error = TEXT("Mapped contact order or 30-second authored-rate limit failed"); return false; }
			Contacts.Add(Time);
		}
		auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("asset"), Clip->GetPathName());
		Row->SetStringField(TEXT("declared_historical_fbx_mapping"), DeclaredFiles[Index]);
		Row->SetStringField(TEXT("local_package_sha1"), Stamp.SHA1);
		Row->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
		Row->SetNumberField(TEXT("source_length_seconds"), Clip->GetPlayLength());
		Row->SetNumberField(TEXT("source_rate_scale"), Clip->RateScale);
		Row->SetNumberField(TEXT("source_frame_rate_numerator"), Clip->GetDataModel()->GetFrameRate().Numerator);
		Row->SetNumberField(TEXT("source_frame_rate_denominator"), Clip->GetDataModel()->GetFrameRate().Denominator);
		Row->SetNumberField(TEXT("segment_start"), Segment.StartPos);
		Row->SetNumberField(TEXT("effective_segment_rate"), EffectiveRate);
		Row->SetNumberField(TEXT("segment_length"), Length);
		SourceRows.Add(MakeShared<FJsonValueObject>(Row));
		Cursor = Segment.GetEndPos(); PlanSegments.Add(Segment);
	}
	FSourceStamp DataStamp; if (!Capture(SourceData, DataStamp, Error)) { return false; } Stamps.Add(DataStamp);
	FSourceStamp SkeletonStamp; if (!Capture(Skeleton, SkeletonStamp, Error)) { return false; } Stamps.Add(SkeletonStamp);
	KeepAlive.Emplace(Skeleton);
	Report->SetArrayField(TEXT("sources"), SourceRows);
	Report->SetStringField(TEXT("source_skill"), SourceData->GetPathName());
	Report->SetStringField(TEXT("source_skill_package_sha1"), DataStamp.SHA1);
	Report->SetStringField(TEXT("source_skeleton_package_sha1"), SkeletonStamp.SHA1);
	Report->SetNumberField(TEXT("planned_montage_length_1x"), Cursor);
	Report->SetArrayField(TEXT("source_local_contacts"), Numbers(Options.Contacts));
	Report->SetArrayField(TEXT("authored_montage_contacts_1x"), Numbers(Contacts));
	if (Options.bRecoverySupplied && (Options.RecoveryEndTime > Cursor
		|| (Options.RecoveryEndTime > 0.0 && !Contacts.IsEmpty() && Options.RecoveryEndTime < Contacts.Last())))
	{ Error = TEXT("Nonzero recovery must be at or after the last mapped contact and no later than the montage end"); return false; }
	if (Options.bContactsSupplied && Options.bRateSupplied)
	{
		TArray<float> FloatTimes; for (double Time : Contacts) { FloatTimes.Add(static_cast<float>(Time)); }
		if (!FRogue10mAuthoredContactTimeline::Validate(FloatTimes.GetData(), FloatTimes.Num(), static_cast<float>(Options.SkillRate), Cursor,
			0.0, Options.bRecoverySupplied ? static_cast<float>(Options.RecoveryEndTime) : 0.0f))
		{ Error = TEXT("Exact runtime contact/recovery kernel rejected the plan before package creation"); return false; }
		if (Options.bRecoverySupplied && (!ValidRecovery(Options.RecoveryEndTime, Contacts.Last(), Options.SkillRate, Cursor)
			|| !ValidRecovery(static_cast<float>(Options.RecoveryEndTime), FloatTimes.Last(), static_cast<float>(Options.SkillRate), Cursor)))
		{ Error = TEXT("Recovery order, float readback, montage end or 30-second authored-rate limit failed before package creation"); return false; }
	}
	Report->SetBoolField(TEXT("recovery_validation_complete"), Options.bContactsSupplied && Options.bRateSupplied && Options.bRecoverySupplied);
	if (!Options.bApply)
	{
		Report->SetBoolField(TEXT("apply_inputs_complete"), Options.bContactsSupplied && Options.bRateSupplied && Options.bRecoverySupplied);
		Report->SetStringField(TEXT("status"), TEXT("plan_only_no_packages_created"));
		return SourcesUnchanged(Stamps, Error);
	}

	// All source, schema, destination and explicit timing preflights precede package creation.
	TArray<UObject*> Created; TArray<TSharedPtr<FJsonValue>> CreatedPaths;
	TArray<TSharedPtr<FJsonValue>> CreatedPackages;
	const auto CreateOwnedPackage = [&](int32 Index)
	{
		auto* Package = CreatePackage(*Packages[Index]);
		if (Package)
		{
			KeepAlive.Emplace(Package);
			CreatedPackages.Add(MakeShared<FJsonValueString>(Package->GetName())); Report->SetArrayField(TEXT("created_packages"), CreatedPackages);
		}
		return Package;
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Package = CreateOwnedPackage(Index);
		if (!Package) { Error = TEXT("CreatePackage failed for sequence"); return false; }
		auto* Copy = Cast<UAnimSequence>(StaticDuplicateObject(Sources[Index], Package, FName(NewNames[Index])));
		if (!Copy) { Error = TEXT("Sequence duplication failed; fresh candidates are retained for inspection"); return false; }
		KeepAlive.Emplace(Copy);
		Copy->SetFlags(RF_Public | RF_Standalone);
		Metadata(*Copy, Sources[Index]->GetPathName(), Stamps[Index].SHA1);
		Created.Add(Copy); CreatedPaths.Add(MakeShared<FJsonValueString>(Copy->GetPathName()));
		Report->SetArrayField(TEXT("created_objects"), CreatedPaths);
		if (!ClipCopyEqual(*Sources[Index], *Copy, Error)) { return false; }
	}
	auto* MontagePackage = CreateOwnedPackage(3);
	if (!MontagePackage) { Error = TEXT("CreatePackage failed for montage"); return false; }
	auto* Montage = NewObject<UAnimMontage>(MontagePackage, FName(NewNames[3]), RF_Public | RF_Standalone);
	KeepAlive.Emplace(Montage);
	Created.Add(Montage); CreatedPaths.Add(MakeShared<FJsonValueString>(Montage->GetPathName()));
	Report->SetArrayField(TEXT("created_objects"), CreatedPaths);
	Montage->SetSkeleton(Skeleton);
	Montage->RateScale = 1.0f; // Contact coordinates are montage seconds at this fixed 1x base.
	Montage->bEnableAutoBlendOut = true;
	Montage->BlendOutTriggerTime = 0.0f; // Start blend-out at the end, so late authored contacts remain active.
	if (Montage->SlotAnimTracks.Num() != 1) { Error = TEXT("Unexpected fresh montage default slot schema"); return false; }
	Montage->SlotAnimTracks[0].SlotName = TEXT("DefaultSlot");
	auto& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	Segments.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FAnimSegment Segment; Segment.SetAnimReference(CastChecked<UAnimSequence>(Created[Index]), true);
		Segment.StartPos = PlanSegments[Index].StartPos;
		Segment.AnimStartTime = 0.0f; Segment.AnimEndTime = PlanSegments[Index].AnimEndTime;
		Segment.AnimPlayRate = 1.0f; Segment.LoopingCount = 1; Segments.Add(Segment);
	}
	Montage->SetCompositeLength(Montage->CalculateSequenceLength());
	Montage->CompositeSections.Reset(); Montage->AddAnimCompositeSection(TEXT("Default"), 0.0f);
	Montage->RefreshCacheData(); Montage->PostEditChange();
	Metadata(*Montage, TEXT("Exact project-owned sequence derivatives; attack -> slash -> slash (3)"), TEXT("See three source records"));
	Report->SetNumberField(TEXT("actual_montage_length_1x"), Montage->GetPlayLength());
	Report->SetNumberField(TEXT("montage_rate_scale"), Montage->RateScale);
	Report->SetBoolField(TEXT("montage_auto_blend_out"), Montage->bEnableAutoBlendOut);
	Report->SetNumberField(TEXT("montage_blend_out_trigger_time"), Montage->BlendOutTriggerTime);
	if (Montage->GetSkeleton() != Skeleton || Montage->HasRootMotion() || Montage->SlotAnimTracks.Num() != 1
		|| Montage->RateScale != 1.0f
		|| !Montage->bEnableAutoBlendOut || Montage->BlendOutTriggerTime != 0.0f
		|| Montage->SlotAnimTracks[0].SlotName != FName(TEXT("DefaultSlot"))
		|| Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 3
		|| Montage->CompositeSections.Num() != 1 || !Montage->Notifies.IsEmpty()
		|| !FMath::IsNearlyEqual(static_cast<double>(Montage->GetPlayLength()), Cursor, 0.0001))
	{ Error = TEXT("Fresh montage structure/root motion/frame-rounded length requires review; nothing saved"); return false; }
	const auto& Section = Montage->CompositeSections[0];
	if (Section.SectionName != FName(TEXT("Default")) || Section.GetTime() != 0.0f
		|| !Section.NextSectionName.IsNone() || !Section.MetaData.IsEmpty())
	{ Error = TEXT("Unexpected fresh Default section linkage/metadata"); return false; }
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[Index];
		if (Segment.GetAnimReference() != Created[Index] || !FMath::IsNearlyEqual(Segment.StartPos, PlanSegments[Index].StartPos)
			|| Segment.AnimStartTime != 0.0f || Segment.AnimEndTime != PlanSegments[Index].AnimEndTime
			|| Segment.AnimPlayRate != 1.0f || Segment.LoopingCount != 1)
		{ Error = TEXT("Composite segment readback mismatch"); return false; }
	}
	auto* DataPackage = CreateOwnedPackage(4);
	if (!DataPackage) { Error = TEXT("CreatePackage failed for skill"); return false; }
	auto* Skill = Cast<URogue10mAttackSkillData>(StaticDuplicateObject(SourceData, DataPackage, FName(NewNames[4])));
	if (!Skill) { Error = TEXT("Skill duplication failed; new candidates remain unsaved"); return false; }
	KeepAlive.Emplace(Skill);
	Skill->SetFlags(RF_Public | RF_Standalone); Created.Add(Skill);
	CreatedPaths.Add(MakeShared<FJsonValueString>(Skill->GetPathName())); Report->SetArrayField(TEXT("created_objects"), CreatedPaths);
	Skill->SkillName = FText::FromString(TEXT("[제작중] 섬멸연참"));
	Skill->SkillDescription = FText::FromString(TEXT("기존 attack, slash, slash (3)을 연결한 정면 3호 제작 후보. 타격번호마다 대상당 1회 판정하며 범위·각도·대상 수는 원본 임시 이월값입니다. 명시 접촉 시각과 재생 속도는 제작값이며, 양손 궤적·접합·실제 3인칭 호 판정·접촉·회복·VFX·밸런스는 미검증입니다."));
	Skill->AllowedSlotPolicy = ERogue10mSkillSlotPolicy::MouseChordOnly;
	Skill->InputSlot = ERogue10mAttackInputSlot::MouseChord;
	Skill->SkillTreeNodeType = ERogue10mSkillTreeNodeType::ActiveTechnique;
	Skill->AttackShape = ERogue10mAttackShape::Arc;
	Skill->HitMode = ERogue10mAttackHitMode::Continuous; Skill->HitCount = 3;
	Skill->bEnableCombo = false; Skill->NextComboSkill = nullptr; Skill->bAdvanceBeforeHit = false;
	Skill->AnimationPlayRate = static_cast<float>(Options.SkillRate);
	Skill->AttackMontage = Montage; Skill->HitStartDelaySeconds = static_cast<float>(Contacts[0]);
	SetContacts(*Skill, *Contact, Contacts);
	Recovery->SetPropertyValue_InContainer(Skill, static_cast<float>(Options.RecoveryEndTime));
	Metadata(*Skill, SourceData->GetPathName(), DataStamp.SHA1);
	const FString SourceContacts = FString::Printf(TEXT("%.17g,%.17g,%.17g"), Options.Contacts[0], Options.Contacts[1], Options.Contacts[2]);
	const FString AuthoredContacts = FString::Printf(TEXT("%.17g,%.17g,%.17g"), Contacts[0], Contacts[1], Contacts[2]);
	const FString AuthoredRate = FString::Printf(TEXT("%.17g"), Options.SkillRate);
	const FString AuthoredRecovery = FString::Printf(TEXT("%.17g"), Options.RecoveryEndTime);
	for (UObject* Object : {static_cast<UObject*>(Montage), static_cast<UObject*>(Skill)})
	{
		auto& Meta = Object->GetOutermost()->GetMetaData();
		Meta.SetValue(Object, TEXT("Rogue10m.G08.SourceLocalContacts"), *SourceContacts);
		Meta.SetValue(Object, TEXT("Rogue10m.G08.AuthoredMontageContacts1x"), *AuthoredContacts);
		Meta.SetValue(Object, TEXT("Rogue10m.G08.AuthoredTemporarySkillRate"), *AuthoredRate);
		Meta.SetValue(Object, TEXT("Rogue10m.G08.AuthoredRecoveryEndTime1x"), *AuthoredRecovery);
		Meta.SetValue(Object, TEXT("Rogue10m.G08.RecoveryMode"), Options.RecoveryEndTime == 0.0 ? TEXT("v1_contact_only") : TEXT("explicit_montage_recovery_end"));
		Meta.SetValue(Object, TEXT("Rogue10m.G08.MontageBlendOutPolicy"), TEXT("auto=true; trigger=0; blend-out starts at montage end"));
		Meta.SetValue(Object, TEXT("Rogue10m.G08.ClipRangePolicy"), TEXT("Full source clips; no final trim or seam approval"));
	}
	if (!CheckContacts(*Skill, *Contact, Contacts, Montage->GetPlayLength(), Recovery->GetPropertyValue_InContainer(Skill), Error)
		|| !CheckRecovery(*Skill, *Recovery, Options.RecoveryEndTime, Contacts, Montage->GetPlayLength(), Error)
		|| !ProtectedDataEqual(*SourceData, *Skill, Error) || !SourcesUnchanged(Stamps, Error)) { return false; }
	Report->SetNumberField(TEXT("readback_recovery_end_time_1x"), Recovery->GetPropertyValue_InContainer(Skill));
	Report->SetBoolField(TEXT("recovery_float_readback_matches"), true);
	if (Skill->AttackShape != ERogue10mAttackShape::Arc || !ValidArcGeometry(Skill->AttackRange, Skill->ArcAngleDegrees))
	{ Error = TEXT("G08 candidate must retain valid source range/angle with explicit Arc shape"); return false; }
	Report->SetBoolField(TEXT("protected_skill_fields_identical"), true);
	Report->SetBoolField(TEXT("damage_cost_cooldown_vfx_verified_unchanged"), true);
	Report->SetBoolField(TEXT("source_packages_unchanged_before_save"), true);
	Report->SetStringField(TEXT("targeting"), TEXT("Continuous per-cut reacquisition; fixed accepted aim; no tracking/instant turn"));
	// Five separate package writes are not atomic. Keep exact partial-save evidence;
	// never remove packages or retry an existing destination automatically.
	TArray<TSharedPtr<FJsonValue>> Saved;
	for (UObject* Object : Created)
	{
		Object->MarkPackageDirty(); FAssetRegistryModule::AssetCreated(Object);
		if (!SaveFresh(*Object, Error)) { Report->SetArrayField(TEXT("saved_objects"), Saved); return false; }
		Saved.Add(MakeShared<FJsonValueString>(Object->GetPathName())); Report->SetArrayField(TEXT("saved_objects"), Saved);
	}
	if (!SourcesUnchanged(Stamps, Error)) { return false; }
	Report->SetStringField(TEXT("status"), TEXT("five_new_packages_saved_prototype_review_pending"));
	Report->SetBoolField(TEXT("cold_reload_verified"), false);
	return true;
}

static void Run(const TArray<FString>& Args)
{
	auto Report = MakeShared<FJsonObject>(); FString Error; FOptions Options;
	Report->SetStringField(TEXT("schema"), Schema);
	bool Passed = IsInGameThread() && GEditor && !GEditor->PlayWorld;
	if (!Passed) { Error = TEXT("Only the owning editor on its game thread outside PIE may author G08"); }
	if (Passed) { Passed = Parse(Args, Options, Error); }
	if (Passed) { Passed = Author(Options, Report, Error); }
	Report->SetBoolField(TEXT("passed"), Passed); Report->SetStringField(TEXT("error"), Error);
	if (!Passed) { Report->SetStringField(TEXT("status"), TEXT("blocked_or_partial_failure_inspect_created_and_saved_lists")); }
	FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	UE_LOG(LogRogue10m, Display, TEXT("G08_AUTHORING_JSON=%s"), *Json);
	UE_LOG(LogRogue10m, Display, TEXT("RESULT=G08_SEQUENCE_%s_%s"), Options.bApply ? TEXT("APPLY") : TEXT("PLAN"), Passed ? TEXT("PASSED") : TEXT("FAILED"));
}

static FAutoConsoleCommand Command(TEXT("Rogue10m.AuthorG08Sequence"),
	TEXT("Plan only by default. plan [contacts=a,b,c] [rate=x] [recovery=t]; apply contacts=a,b,c rate=x recovery=t creates five fresh unassigned G08 v2 prototypes. Contacts are source-local seconds; recovery is absolute montage-1x seconds, explicit 0 keeps V1 contact-only behavior. No guessed timing."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mG08AuthoringArgumentsTest, "Rogue10m.Authoring.G08.ExplicitInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mG08AuthoringArgumentsTest::RunTest(const FString&)
{
	using namespace Rogue10mG08SequenceAuthoring;
	FString Error; FOptions Plan;
	TestTrue(TEXT("Empty command is a plan"), Parse({}, Plan, Error)); TestFalse(TEXT("Plan never opts into apply"), Plan.bApply);
	TestFalse(TEXT("Empty plan never claims an explicit recovery input"), Plan.bRecoverySupplied);
	FOptions PlanRecovery; TestTrue(TEXT("Plan accepts optional explicit recovery without applying"), Parse({TEXT("plan"), TEXT("recovery=0")}, PlanRecovery, Error));
	TestTrue(TEXT("Plan records recovery without creating apply intent"), PlanRecovery.bRecoverySupplied && !PlanRecovery.bApply);
	FOptions Missing; TestFalse(TEXT("Apply requires explicit values"), Parse({TEXT("apply")}, Missing, Error));
	FOptions MissingRecovery; TestFalse(TEXT("Apply requires explicit recovery even with contacts and rate"), Parse({TEXT("apply"), TEXT("contacts=0.2,0.3,0.4"), TEXT("rate=1")}, MissingRecovery, Error));
	FOptions Valid; TestTrue(TEXT("Explicit local offsets/rate/recovery parse"), Parse({TEXT("apply"), TEXT("contacts=0.2,0.3,0.4"), TEXT("rate=1"), TEXT("recovery=2")}, Valid, Error));
	FOptions Zero; TestTrue(TEXT("Explicit zero recovery keeps V1 contact-only opt-out"), Parse({TEXT("apply"), TEXT("contacts=0.2,0.3,0.4"), TEXT("rate=1"), TEXT("recovery=0")}, Zero, Error));
	TestTrue(TEXT("Zero recovery is explicitly recorded"), Zero.bRecoverySupplied && Zero.RecoveryEndTime == 0.0);
	FOptions NegativeRecovery; TestFalse(TEXT("Negative recovery is rejected"), Parse({TEXT("recovery=-0.1")}, NegativeRecovery, Error));
	FOptions NaNRecovery; TestFalse(TEXT("NaN recovery is rejected"), Parse({TEXT("recovery=nan")}, NaNRecovery, Error));
	FOptions InfiniteRecovery; TestFalse(TEXT("Infinite recovery is rejected"), Parse({TEXT("recovery=inf")}, InfiniteRecovery, Error));
	FOptions UnderflowRecovery; TestFalse(TEXT("Positive recovery cannot silently become zero after float conversion"), Parse({TEXT("recovery=1e-50")}, UnderflowRecovery, Error));
	FOptions DuplicateRecovery; TestFalse(TEXT("Duplicate recovery cannot replace an author input"), Parse({TEXT("recovery=0"), TEXT("recovery=2")}, DuplicateRecovery, Error));
	FOptions Negative; TestFalse(TEXT("Reverse skill playback is rejected"), Parse({TEXT("apply"), TEXT("contacts=0.2,0.3,0.4"), TEXT("rate=-1"), TEXT("recovery=0")}, Negative, Error));
	FOptions Duplicate; TestFalse(TEXT("Duplicate values cannot silently replace an author input"), Parse({TEXT("rate=1"), TEXT("rate=2")}, Duplicate, Error));
	FOptions Count; TestFalse(TEXT("Three contact values are required"), Parse({TEXT("contacts=0.1,0.2")}, Count, Error));
	TestTrue(TEXT("Recovery may equal the last contact"), ValidRecovery(2.0, 2.0, 1.0, 3.0));
	TestTrue(TEXT("Recovery may equal montage end"), ValidRecovery(3.0, 2.0, 1.0, 3.0));
	TestTrue(TEXT("Zero opts out of explicit recovery ordering"), ValidRecovery(0.0, 2.0, 1.0, 3.0));
	TestFalse(TEXT("Recovery before last contact is rejected"), ValidRecovery(1.9, 2.0, 1.0, 3.0));
	TestFalse(TEXT("Recovery beyond montage end is rejected"), ValidRecovery(3.1, 2.0, 1.0, 3.0));
	TestFalse(TEXT("Recovery beyond 30 scaled seconds is rejected"), ValidRecovery(3.0, 2.0, 0.09, 3.0));
	TestTrue(TEXT("Positive range and boundary 180-degree Arc are supported"), ValidArcGeometry(150.0, 180.0));
	TestFalse(TEXT("Zero range cannot author Arc geometry"), ValidArcGeometry(0.0, 90.0));
	TestFalse(TEXT("Zero Arc angle cannot author geometry"), ValidArcGeometry(150.0, 0.0));
	TestFalse(TEXT("Arc angle above 180 degrees is rejected"), ValidArcGeometry(150.0, 180.001));
	TestFalse(TEXT("Nonfinite range is rejected"), ValidArcGeometry(std::numeric_limits<double>::infinity(), 90.0));
	TestFalse(TEXT("Nonfinite Arc angle is rejected"), ValidArcGeometry(150.0, std::numeric_limits<double>::quiet_NaN()));
	return true;
}
#endif
#endif
