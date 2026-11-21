#pragma once

#include "InputCoreTypes.h"
#include "Internationalization/Text.h"

/** Shared native input contract. This is not a save schema or a four-slot preset definition. */
namespace Rogue10mInput
{
inline const FKey& DodgeKey() { return EKeys::LeftControl; }
inline FText DodgeKeyLabel() { return NSLOCTEXT("Rogue10mInput", "DodgeKey", "Ctrl"); }
inline const FKey& IdentityKey() { return EKeys::V; }
inline FText IdentityKeyLabel() { return NSLOCTEXT("Rogue10mInput", "IdentityKey", "V"); }
inline const FKey& InspectionCameraKey() { return EKeys::C; }
}
