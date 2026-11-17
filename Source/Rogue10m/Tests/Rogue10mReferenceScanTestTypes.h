// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "StructUtils/SharedStruct.h"
#include "UObject/FieldPath.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mReferenceScanTestTypes.generated.h"

/** Its name deliberately resembles a native property bag; its storage is ordinary reflection. */
USTRUCT()
struct FRogue10mReferenceScanPropertyBagLookalike
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UObject> Target = nullptr;
};

/** A payload with native dynamic storage below reflected fields. No gameplay or asset behavior. */
USTRUCT()
struct FRogue10mReferenceScanPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UObject> Strong = nullptr;

	UPROPERTY()
	TSoftObjectPtr<UObject> Soft;

	UPROPERTY()
	FInstancedStruct Nested;

	UPROPERTY()
	FSharedStruct Cycle;
};

/** Real engine wrappers also inherit FInstancedStruct; both storage and added fields matter. */
USTRUCT()
struct FRogue10mReferenceScanDerivedStorage : public FInstancedStruct
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UObject> Additional = nullptr;
};

/** Transient-only reference fixtures. The automation tests never save, spawn actors, or enter PIE. */
UCLASS(Transient)
class URogue10mReferenceScanTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UObject> Strong = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UObject> Weak;

	UPROPERTY()
	TLazyObjectPtr<UObject> Lazy;

	UPROPERTY()
	TSoftObjectPtr<UObject> Soft;

	UPROPERTY()
	TFieldPath<FProperty> Field;

	UPROPERTY()
	TOptional<TObjectPtr<UObject>> OptionalObject;

	UPROPERTY()
	TOptional<FRogue10mReferenceScanPropertyBagLookalike> OptionalStruct;

	UPROPERTY()
	FInstancedStruct Instanced;

	UPROPERTY()
	FSharedStruct Shared;

	UPROPERTY()
	FConstSharedStruct ConstShared;

	UPROPERTY()
	FInstancedPropertyBag Bag;

	UPROPERTY()
	FRogue10mReferenceScanPropertyBagLookalike Lookalike;

	UPROPERTY()
	FRogue10mReferenceScanDerivedStorage Derived;
};
