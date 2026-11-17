#pragma once

/** Portable run-local narrative clue rules. Source validation and dialogue belong to the caller. */
namespace Rogue10mVillageQuest
{
enum class EOptionalClueState
{
	Missing,
	Found,
	Analyzed,
	Reported
};

/** Optional V1 -> V4 -> V1 narrative; it owns no primary quest progress or rewards. */
class FOptionalClueModel
{
public:
	void Reset() noexcept
	{
		State = EOptionalClueState::Missing;
	}

	EOptionalClueState GetState() const noexcept
	{
		return State;
	}

	bool ObserveSource() noexcept
	{
		if (State != EOptionalClueState::Missing) { return false; }
		State = EOptionalClueState::Found;
		return true;
	}

	bool Analyze() noexcept
	{
		if (State != EOptionalClueState::Found) { return false; }
		State = EOptionalClueState::Analyzed;
		return true;
	}

	bool Report() noexcept
	{
		if (State != EOptionalClueState::Analyzed) { return false; }
		State = EOptionalClueState::Reported;
		return true;
	}

private:
	EOptionalClueState State = EOptionalClueState::Missing;
};
}
