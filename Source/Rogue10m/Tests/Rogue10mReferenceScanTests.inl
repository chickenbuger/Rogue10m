// Included after the production scanner in the same translation unit to test its real implementation.
// No alternative scanner, editor map, disk fixture, or private engine mutation is used here.

#include "Tests/Rogue10mReferenceScanTestTypes.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/StructView.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Rogue10mReferenceScanTests
{
	using namespace Rogue10mCrop20261114Dependencies;

	struct FReadState
	{
		TMap<const UObject*, EObjectFlags> Flags;
		TMap<const UObject*, EInternalObjectFlags> InternalFlags;
		TMap<const UObject*, FGuid> Guids;
		TMap<const UPackage*, bool> PackageDirty;

		FReadState()
		{
			for (TObjectIterator<UObject> It; It; ++It)
			{
				const UObject* Object = *It;
				Flags.Add(Object, Object->GetFlags());
				InternalFlags.Add(Object, Object->GetInternalFlags());
				// This constructor only reads the annotation; GetOrCreateIDForObject is never used in a scan.
				Guids.Add(Object, FUniqueObjectGuid(Object).GetGuid());
				if (const UPackage* Package = Cast<UPackage>(Object)) { PackageDirty.Add(Package, Package->IsDirty()); }
			}
		}
	};

	template<typename K, typename V>
	bool SameMap(const TMap<K, V>& A, const TMap<K, V>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (const auto& Pair : A)
		{
			const V* Other = B.Find(Pair.Key);
			if (!Other || *Other != Pair.Value) { return false; }
		}
		return true;
	}

	void CheckReadOnly(FAutomationTestBase& Test, const FReadState& Before)
	{
		const FReadState After;
		Test.TestTrue(TEXT("Loaded object identities and object flags are unchanged"), SameMap(Before.Flags, After.Flags));
		Test.TestTrue(TEXT("Internal object flags are unchanged"), SameMap(Before.InternalFlags, After.InternalFlags));
		Test.TestTrue(TEXT("Existing GUID annotations are unchanged; none were generated"), SameMap(Before.Guids, After.Guids));
		Test.TestTrue(TEXT("Every loaded package keeps its dirty flag"), SameMap(Before.PackageDirty, After.PackageDirty));
	}

	void Index(FReferenceScan& Scan)
	{
		TArray<const UObject*> Objects;
		for (TObjectIterator<UObject> It; It; ++It) { Objects.Add(*It); }
		Scan.IndexLoadedGuids(Objects);
	}

	bool Has(const FReferenceScan& Scan, const TCHAR* PropertyPrefix, const FString& Target, const TCHAR* Kind = nullptr)
	{
		return Scan.Rows.ContainsByPredicate([&](const FReferenceRow& Row)
		{
			return Row.Property.StartsWith(PropertyPrefix) && Row.Target == Target && (!Kind || Row.Kind == Kind);
		});
	}

	void AddTarget(FReferenceScan& Scan, const UObject* Object)
	{
		Scan.Targets.Add(Object);
		Scan.TargetPaths.Add(Object->GetPathName());
	}

	FProperty* Property(const TCHAR* Name)
	{
		return FindFProperty<FProperty>(URogue10mReferenceScanTestObject::StaticClass(), Name);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mReferenceScanPointersTest,
	"Rogue10m.Editor.ReferenceScan.PointersAndOptionalReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mReferenceScanPointersTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mReferenceScanTests;
	TStrongObjectPtr<URogue10mReferenceScanTestObject> Source(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	TStrongObjectPtr<UObject> Target(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	TStrongObjectPtr<UObject> Unrelated(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	Source->Strong = Target.Get(); Source->Weak = Target.Get(); Source->Lazy = Target.Get(); Source->Soft = Target.Get();
	Source->OptionalObject.Emplace(Target.Get());
	Source->OptionalStruct.Emplace().Target = Target.Get();
	Source->Field = TFieldPath<FProperty>(Property(TEXT("Strong")));
	const FString FieldBefore = Source->Field.ToString();
	const FUniqueObjectGuid LazyBefore = Source->Lazy.GetUniqueID();
	const FString TargetPath = Target->GetPathName();
	FReferenceScan Scan;
	AddTarget(Scan, Target.Get()); AddTarget(Scan, Source->GetClass());
	const FReadState Before;
	Index(Scan); Scan.Scan(Source.Get(), false);
	CheckReadOnly(*this, Before);
	TestTrue(TEXT("Strong reference reaches the requested object"), Has(Scan, TEXT("Strong"), TargetPath, TEXT("object_handle_path")));
	TestTrue(TEXT("Weak reference reaches the requested object"), Has(Scan, TEXT("Weak"), TargetPath, TEXT("weak_object")));
	TestTrue(TEXT("Loaded lazy GUID reaches the indexed object"), Has(Scan, TEXT("Lazy"), TargetPath, TEXT("lazy_guid_loaded")));
	TestTrue(TEXT("Soft reference is read as a path"), Has(Scan, TEXT("Soft"), TargetPath, TEXT("soft_object_path")));
	TestTrue(TEXT("Set optional object is traversed"), Has(Scan, TEXT("OptionalObject"), TargetPath));
	TestTrue(TEXT("Set optional reflected struct is traversed"), Has(Scan, TEXT("OptionalStruct"), TargetPath));
	TestTrue(TEXT("Field path keeps its loaded owning class"), Has(Scan, TEXT("Field"), Source->GetClass()->GetPathName(), TEXT("field_path_owner")));
	TestEqual(TEXT("Field path storage is unchanged"), Source->Field.ToString(), FieldBefore);
	TestTrue(TEXT("Lazy GUID storage is unchanged"), Source->Lazy.GetUniqueID() == LazyBefore);
	TestFalse(TEXT("Indexing does not create GUIDs on unrelated objects"), FUniqueObjectGuid(Unrelated.Get()).IsValid());
	TestEqual(TEXT("Known pointer representations are supported"), Scan.Unsupported.Num(), 0);
	TestFalse(TEXT("Fixture scan stays within limits"), Scan.bTruncated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mReferenceScanDynamicTest,
	"Rogue10m.Editor.ReferenceScan.DynamicStorageAndOrdinaryLookalike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mReferenceScanDynamicTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mReferenceScanTests;
	TStrongObjectPtr<URogue10mReferenceScanTestObject> Source(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	TStrongObjectPtr<UObject> Target(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	const FString TargetPath = Target->GetPathName();
	FRogue10mReferenceScanPayload Payload;
	Payload.Strong = Target.Get(); Payload.Soft = Target.Get();
	FRogue10mReferenceScanPropertyBagLookalike Inner; Inner.Target = Target.Get();
	Payload.Nested.InitializeAs<FRogue10mReferenceScanPropertyBagLookalike>(Inner);
	Source->Instanced.InitializeAs<FRogue10mReferenceScanPayload>(Payload);
	Source->Shared = FSharedStruct::Make(Payload);
	Source->ConstShared = Source->Shared;
	Source->Lookalike = Inner;
	Source->Derived.InitializeAs<FRogue10mReferenceScanPayload>(Payload);
	Source->Derived.Additional = Target.Get();
	Source->Bag.AddProperties({FPropertyBagPropertyDesc(TEXT("Target"), EPropertyBagPropertyType::Object, UObject::StaticClass()),
		FPropertyBagPropertyDesc(TEXT("SoftTarget"), EPropertyBagPropertyType::SoftObject, UObject::StaticClass())});
	if (!TestTrue(TEXT("Set dynamic bag object fixture"), Source->Bag.SetValueObject(TEXT("Target"), Target.Get()) == EPropertyBagResult::Success)
		|| !TestTrue(TEXT("Set dynamic bag soft fixture"), Source->Bag.SetValueSoftPath(TEXT("SoftTarget"), FSoftObjectPath(Target.Get())) == EPropertyBagResult::Success)) { return false; }
	const void* InstancedMemory = Source->Instanced.GetMemory();
	const void* SharedMemory = Source->Shared.GetMemory();
	const void* BagMemory = Source->Bag.GetValue().GetMemory();
	FReferenceScan Scan;
	AddTarget(Scan, Target.Get());
	const FReadState Before;
	Scan.Scan(Source.Get(), false);
	CheckReadOnly(*this, Before);
	TestTrue(TEXT("Instanced payload object found"), Has(Scan, TEXT("Instanced"), TargetPath, TEXT("object_handle_path")));
	TestTrue(TEXT("Nested instanced payload found"), Scan.Rows.ContainsByPredicate([&](const FReferenceRow& Row) { return Row.Property.StartsWith(TEXT("Instanced")) && Row.Property.Contains(TEXT("Nested")) && Row.Target == TargetPath; }));
	TestTrue(TEXT("Shared payload object found"), Has(Scan, TEXT("Shared"), TargetPath));
	TestTrue(TEXT("Const shared payload object found"), Has(Scan, TEXT("ConstShared"), TargetPath));
	TestTrue(TEXT("Dynamic bag object found"), Has(Scan, TEXT("Bag"), TargetPath, TEXT("object_handle_path")));
	TestTrue(TEXT("Dynamic bag soft object found"), Has(Scan, TEXT("Bag"), TargetPath, TEXT("soft_object_path")));
	TestTrue(TEXT("Ordinary reflected struct is not rejected for its PropertyBag name"), Has(Scan, TEXT("Lookalike"), TargetPath));
	TestTrue(TEXT("Inherited native dynamic storage is traversed"), Scan.Rows.ContainsByPredicate([&](const FReferenceRow& Row) { return Row.Property.StartsWith(TEXT("Derived")) && Row.Property.Contains(TEXT(".$value")) && Row.Target == TargetPath; }));
	TestTrue(TEXT("A wrapper subclass also keeps its own reflected fields"), Has(Scan, TEXT("Derived[0].Additional"), TargetPath));
	TestTrue(TEXT("Instanced memory is unchanged"), InstancedMemory == Source->Instanced.GetMemory());
	TestTrue(TEXT("Shared memory is unchanged"), SharedMemory == Source->Shared.GetMemory() && SharedMemory == Source->ConstShared.GetMemory());
	TestTrue(TEXT("Bag memory is unchanged"), BagMemory == Source->Bag.GetValue().GetMemory());
	TestTrue(TEXT("Dynamic payload values are unchanged"), Source->Instanced.Get<FRogue10mReferenceScanPayload>().Strong == Target.Get()
		&& Source->Shared.Get<FRogue10mReferenceScanPayload>().Strong == Target.Get() && Source->Lookalike.Target == Target.Get());
	TestEqual(TEXT("Known dynamic storage and ordinary metadata are supported"), Scan.Unsupported.Num(), 0);
	TestFalse(TEXT("Dynamic fixture scan stays within limits"), Scan.bTruncated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mReferenceScanEmptyTest,
	"Rogue10m.Editor.ReferenceScan.EmptyStorageAndUnresolvedGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mReferenceScanEmptyTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mReferenceScanTests;
	TStrongObjectPtr<URogue10mReferenceScanTestObject> Source(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	Source->Lazy = FUniqueObjectGuid(FGuid::NewGuid());
	const FUniqueObjectGuid GuidBefore = Source->Lazy.GetUniqueID();
	const FReadState Before;
	FReferenceScan Scan;
	Index(Scan); Scan.Scan(Source.Get(), true);
	CheckReadOnly(*this, Before);
	TestTrue(TEXT("Unloaded GUID is preserved in a diagnostic row"), Scan.Rows.ContainsByPredicate([&](const FReferenceRow& Row) { return Row.Property.StartsWith(TEXT("Lazy")) && Row.Kind == TEXT("lazy_guid_unresolved") && Row.Target == GuidBefore.GetGuid().ToString(); }));
	TestTrue(TEXT("Reading an unresolved GUID does not rewrite the lazy pointer"), Source->Lazy.GetUniqueID() == GuidBefore);
	TestFalse(TEXT("Unset optional object is left unset"), Source->OptionalObject.IsSet());
	TestFalse(TEXT("Unset optional struct is left unset"), Source->OptionalStruct.IsSet());
	TestFalse(TEXT("Unset optionals do not produce child references"), Scan.Rows.ContainsByPredicate([](const FReferenceRow& Row) { return Row.Property.StartsWith(TEXT("Optional")); }));
	TestFalse(TEXT("Empty instanced struct stays empty"), Source->Instanced.IsValid());
	TestFalse(TEXT("Empty shared struct stays empty"), Source->Shared.IsValid());
	TestFalse(TEXT("Empty const shared struct stays empty"), Source->ConstShared.IsValid());
	TestFalse(TEXT("Empty bag stays empty"), Source->Bag.GetValue().IsValid());
	TestEqual(TEXT("Empty supported values are not unsupported reflection errors"), Scan.Unsupported.Num(), 0);
	TestFalse(TEXT("Empty scan stays within limits"), Scan.bTruncated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mReferenceScanCycleTest,
	"Rogue10m.Editor.ReferenceScan.SharedCycleAndAliasPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mReferenceScanCycleTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mReferenceScanTests;
	TStrongObjectPtr<URogue10mReferenceScanTestObject> Source(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	TStrongObjectPtr<UObject> Target(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	Source->Shared.InitializeAs<FRogue10mReferenceScanPayload>();
	auto& Payload = Source->Shared.Get<FRogue10mReferenceScanPayload>();
	Payload.Strong = Target.Get(); Payload.Cycle = Source->Shared;
	Source->ConstShared = Source->Shared;
	const void* MemoryBefore = Source->Shared.GetMemory();
	const FReadState Before;
	FReferenceScan Scan;
	Scan.Scan(Source.Get(), true);
	CheckReadOnly(*this, Before);
	TestTrue(TEXT("The first shared path reaches the object"), Has(Scan, TEXT("Shared"), Target->GetPathName()));
	TestTrue(TEXT("An alias keeps its own property path"), Has(Scan, TEXT("ConstShared"), Target->GetPathName()));
	TestTrue(TEXT("A shared cycle produces an explicit backedge"), Scan.Rows.ContainsByPredicate([](const FReferenceRow& Row) { return Row.Kind == TEXT("dynamic_struct_cycle"); }));
	TestTrue(TEXT("Cycle inspection keeps the memory and backedge intact"), MemoryBefore == Source->Shared.GetMemory() && MemoryBefore == Payload.Cycle.GetMemory());
	TestEqual(TEXT("Cycle is supported rather than an opaque-storage error"), Scan.Unsupported.Num(), 0);
	TestFalse(TEXT("Cycle terminates without the depth/value limit"), Scan.bTruncated);
	// Explicit fixture teardown after all preservation checks prevents a self-owned shared allocation leak.
	Payload.Cycle.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mReferenceScanLimitsTest,
	"Rogue10m.Editor.ReferenceScan.TraversalLimitsRemainFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mReferenceScanLimitsTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mReferenceScanTests;
	TStrongObjectPtr<URogue10mReferenceScanTestObject> Source(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	TStrongObjectPtr<UObject> Target(NewObject<URogue10mReferenceScanTestObject>(GetTransientPackage()));
	Source->Strong = Target.Get();
	FProperty* Strong = Property(TEXT("Strong"));
	if (!TestNotNull(TEXT("Reflected fixture object property"), Strong)) { return false; }
	const void* Data = Strong->ContainerPtrToValuePtr<void>(Source.Get());
	FReferenceScan TooDeep, OverQuota;
	OverQuota.ValuesVisited = MaxReferenceValues;
	const FReadState Before;
	TooDeep.Value(Source.Get(), Strong, Data, TEXT("DepthBoundary"), MaxReferenceDepth + 1, true);
	OverQuota.Value(Source.Get(), Strong, Data, TEXT("ValueBoundary"), 0, true);
	// Once incomplete, a scan may not resume and accidentally report partial references as complete.
	TooDeep.Scan(Source.Get(), true); OverQuota.Scan(Source.Get(), true);
	CheckReadOnly(*this, Before);
	TestTrue(TEXT("Depth exhaustion remains an incomplete scan"), TooDeep.bTruncated);
	TestTrue(TEXT("Value exhaustion remains an incomplete scan"), OverQuota.bTruncated);
	TestEqual(TEXT("Depth-exhausted scan emits no successful reference"), TooDeep.Rows.Num(), 0);
	TestEqual(TEXT("Value-exhausted scan emits no successful reference"), OverQuota.Rows.Num(), 0);
	TestTrue(TEXT("Fail-closed traversal leaves the input pointer unchanged"), Source->Strong == Target.Get());
	return true;
}
