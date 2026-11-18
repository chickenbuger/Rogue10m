#pragma once

/** Portable run-local quest rules. World observations and reward grants belong to the caller. */
namespace Rogue10mVillageQuest
{
constexpr int QuestCount = 5;
constexpr int ObjectiveCount = 2;

enum class EState
{
	Unavailable,
	Available,
	Active,
	Ready,
	Claimed
};

class FQuestModel
{
public:
	void Reset()
	{
		for (FQuestEntry& Quest : Quests)
		{
			Quest = {};
		}
	}

	EState GetState(int QuestIndex) const
	{
		if (!IsValidQuestIndex(QuestIndex)) { return EState::Unavailable; }
		const FQuestEntry& Quest = Quests[QuestIndex];
		if (Quest.bClaimed) { return EState::Claimed; }
		if (!Quest.bAccepted) { return EState::Available; }
		return GetProgress(QuestIndex) == ObjectiveCount ? EState::Ready : EState::Active;
	}

	bool Accept(int QuestIndex)
	{
		if (GetState(QuestIndex) != EState::Available) { return false; }
		Quests[QuestIndex].bAccepted = true;
		return true;
	}

	/** Returns true only when a new valid fact is recorded, including before acceptance. */
	bool Observe(int QuestIndex, int ObjectiveIndex)
	{
		if (!IsValidQuestIndex(QuestIndex) || ObjectiveIndex < 0 || ObjectiveIndex >= ObjectiveCount)
		{
			return false;
		}
		bool& bObserved = Quests[QuestIndex].bObserved[ObjectiveIndex];
		if (bObserved) { return false; }
		bObserved = true;
		return true;
	}

	int GetProgress(int QuestIndex) const
	{
		if (!IsValidQuestIndex(QuestIndex)) { return 0; }
		int Progress = 0;
		for (bool bObserved : Quests[QuestIndex].bObserved)
		{
			if (bObserved) { ++Progress; }
		}
		return Progress;
	}

	/** Facts remain inspectable before acceptance and after reporting a quest. */
	bool HasObserved(int QuestIndex, int ObjectiveIndex) const
	{
		return IsValidQuestIndex(QuestIndex) && ObjectiveIndex >= 0 && ObjectiveIndex < ObjectiveCount
			&& Quests[QuestIndex].bObserved[ObjectiveIndex];
	}

	/** Reserve before calling external reward code: a callback cannot start a second grant. */
	bool BeginClaim(int QuestIndex)
	{
		if (GetState(QuestIndex) != EState::Ready || Quests[QuestIndex].bClaimPending) { return false; }
		Quests[QuestIndex].bClaimPending = true;
		return true;
	}

	/** Failed grants retain acceptance and observations, so a revisit can retry the reward. */
	void FinishClaim(int QuestIndex, bool bSucceeded)
	{
		if (!IsClaimPending(QuestIndex)) { return; }
		FQuestEntry& Quest = Quests[QuestIndex];
		Quest.bClaimPending = false;
		if (bSucceeded) { Quest.bClaimed = true; }
	}

	bool IsClaimPending(int QuestIndex) const
	{
		return IsValidQuestIndex(QuestIndex) && Quests[QuestIndex].bClaimPending;
	}

private:
	struct FQuestEntry
	{
		bool bAccepted = false;
		bool bObserved[ObjectiveCount] = {};
		bool bClaimPending = false;
		bool bClaimed = false;
	};

	static bool IsValidQuestIndex(int QuestIndex)
	{
		return QuestIndex >= 0 && QuestIndex < QuestCount;
	}

	FQuestEntry Quests[QuestCount] = {};
};
}
