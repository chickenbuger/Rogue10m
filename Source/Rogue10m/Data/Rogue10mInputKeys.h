#pragma once

#include "InputCoreTypes.h"
#include "Internationalization/Text.h"

/** Shared native input contract. This is not a save schema or a four-slot preset definition. */
namespace Rogue10mInput
{
inline const FKey& DodgeKey() { return EKeys::LeftAlt; }
inline FText DodgeKeyLabel() { return NSLOCTEXT("Rogue10mInput", "DodgeKey", "Alt"); }
}
