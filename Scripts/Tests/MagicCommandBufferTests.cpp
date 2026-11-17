#include "Magic/Rogue10mMagicCommandBuffer.h"

#include <cstdlib>
#include <iostream>

using namespace Rogue10m::Magic;
using EToken = ECommandToken;
using EStatus = ECommandKeyStatus;

static int Checks = 0;
static int Cases = 0;

#define CHECK(Condition) do { ++Checks; if (!(Condition)) { \
	std::cerr << "FAILED line=" << __LINE__ << " expression=" << #Condition << '\n'; \
	std::exit(1); } } while (false)

static void InactiveAndOrder()
{
	++Cases;
	TCommandBuffer<8> Buffer;
	CHECK(!Buffer.IsActive());
	const auto Inactive = Buffer.KeyDown(Buffer.GetEpoch(), EToken::Q);
	CHECK(Inactive.Status == EStatus::Inactive && !Inactive.bConsumed);
	CHECK(Buffer.ReadSnapshot().Count == 0);
	const auto Epoch = Buffer.BeginSession(0);
	const std::array<EToken, 6> Expected{EToken::Q, EToken::E, EToken::R, EToken::R, EToken::E, EToken::Q};
	for (const auto Token : Expected)
	{
		const auto Down = Buffer.KeyDown(Epoch, Token);
		CHECK(Down.Status == EStatus::Appended && Down.bConsumed);
		CHECK(Buffer.KeyUp(Epoch, Token).Status == EStatus::Released);
	}
	const auto Snapshot = Buffer.ReadSnapshot();
	CHECK(Snapshot.bActive && Snapshot.Count == Expected.size() && Snapshot.Epoch == Epoch);
	for (std::size_t Index = 0; Index < Expected.size(); ++Index) { CHECK(Snapshot.Tokens[Index] == Expected[Index]); }
}

static void RepeatedAndHeldKeys()
{
	++Cases;
	const std::array<EToken, 3> Keys{EToken::Q, EToken::E, EToken::R};
	for (std::uint8_t HeldMask = 0; HeldMask < 8; ++HeldMask)
	{
		TCommandBuffer<6> Buffer;
		const auto Epoch = Buffer.BeginSession(HeldMask);
		std::size_t ExpectedCount = 0;
		for (const auto Token : Keys)
		{
			const bool WasHeld = (HeldMask & Buffer.KeyMask(Token)) != 0;
			const auto First = Buffer.KeyDown(Epoch, Token);
			CHECK(First.Status == (WasHeld ? EStatus::Repeated : EStatus::Appended));
			CHECK(First.bConsumed);
			if (!WasHeld) { ++ExpectedCount; }
			CHECK(Buffer.KeyDown(Epoch, Token).Status == EStatus::Repeated);
			CHECK(Buffer.KeyUp(Epoch, Token).bConsumed);
			CHECK(Buffer.KeyDown(Epoch, Token).Status == EStatus::Appended);
			++ExpectedCount;
		}
		CHECK(Buffer.ReadSnapshot().Count == ExpectedCount);
	}
}

static void OldEpochDoesNotReleaseNewKeys()
{
	++Cases;
	TCommandBuffer<4> Buffer;
	const auto Old = Buffer.BeginSession(0);
	CHECK(Buffer.KeyDown(Old, EToken::Q).Status == EStatus::Appended);
	const auto Current = Buffer.BeginSession(Buffer.KeyMask(EToken::Q));
	CHECK(Current != Old);
	CHECK(Buffer.KeyUp(Old, EToken::Q).Status == EStatus::StaleSession);
	CHECK(Buffer.KeyDown(Current, EToken::Q).Status == EStatus::Repeated);
	const auto Stale = Buffer.KeyDown(Old, EToken::E);
	CHECK(Stale.Status == EStatus::StaleSession && Stale.bConsumed);
	CHECK(Buffer.KeyDown(Current, EToken::E).Status == EStatus::Appended);
	CHECK(!Buffer.CancelSession(Old));
	CHECK(Buffer.ReadSnapshot().Count == 1);
	CHECK(Buffer.KeyUp(Current, EToken::Q).Status == EStatus::Released);
	CHECK(Buffer.KeyDown(Current, EToken::Q).Status == EStatus::Appended);
}

static void FullIsConsumedAndNeverAutoSubmits()
{
	++Cases;
	TCommandBuffer<1> Buffer; // Storage fixture, not the game's maximum spell length.
	const auto Epoch = Buffer.BeginSession(0);
	CHECK(Buffer.KeyDown(Epoch, EToken::Q).Status == EStatus::Appended);
	const auto Full = Buffer.KeyDown(Epoch, EToken::R);
	CHECK(Full.Status == EStatus::Full && Full.bConsumed);
	CHECK(Buffer.IsActive());
	CHECK(Buffer.KeyDown(Epoch, EToken::R).Status == EStatus::Repeated);
	CHECK(Buffer.KeyUp(Epoch, EToken::R).Status == EStatus::Released);
	CHECK(Buffer.KeyDown(Epoch, EToken::R).Status == EStatus::Full);
	const auto Snapshot = Buffer.ReadSnapshot();
	CHECK(Snapshot.Count == 1 && Snapshot.Tokens[0] == EToken::Q);
}

static void CancelAndSnapshotIsolation()
{
	++Cases;
	TCommandBuffer<4> Buffer;
	const auto Epoch = Buffer.BeginSession(0);
	CHECK(Buffer.KeyDown(Epoch, EToken::E).Status == EStatus::Appended);
	auto Snapshot = Buffer.ReadSnapshot();
	Snapshot.Tokens[0] = EToken::R;
	CHECK(Buffer.ReadSnapshot().Tokens[0] == EToken::E);
	Snapshot = Buffer.ReadSnapshot();
	CHECK(Buffer.CancelSession(Epoch));
	CHECK(!Buffer.CancelSession(Epoch));
	CHECK(!Buffer.IsActive() && Buffer.ReadSnapshot().Count == 0);
	CHECK(Snapshot.bActive && Snapshot.Count == 1 && Snapshot.Tokens[0] == EToken::E);
	CHECK(Buffer.KeyDown(Epoch, EToken::E).Status == EStatus::StaleSession);
	CHECK(Buffer.KeyDown(Epoch, EToken::E).bConsumed);
	CHECK(Buffer.KeyUp(Epoch, EToken::E).bConsumed);
	const auto New = Buffer.BeginSession(Buffer.KeyMask(EToken::E));
	CHECK(Buffer.KeyDown(New, EToken::E).Status == EStatus::Repeated);
	CHECK(Buffer.KeyUp(New, EToken::E).Status == EStatus::Released);
	CHECK(Buffer.KeyDown(New, EToken::E).Status == EStatus::Appended);
}

static void InvalidValuesAreSafe()
{
	++Cases;
	TCommandBuffer<2> Buffer;
	const auto Epoch = Buffer.BeginSession(255); // Unknown physical mask bits are ignored.
	for (int Value = 3; Value < 256; ++Value)
	{
		const auto Invalid = static_cast<EToken>(Value);
		CHECK(Buffer.KeyMask(Invalid) == 0);
		CHECK(Buffer.KeyDown(Epoch, Invalid).Status == EStatus::InvalidToken);
		CHECK(Buffer.KeyUp(Epoch, Invalid).Status == EStatus::InvalidToken);
		CHECK(Buffer.ReadSnapshot().Count == 0);
	}
	CHECK(Buffer.KeyDown(Epoch, EToken::Q).Status == EStatus::Repeated);
	CHECK(Buffer.KeyUp(Epoch, EToken::Q).Status == EStatus::Released);
	CHECK(Buffer.KeyDown(Epoch, EToken::Q).Status == EStatus::Appended);
	CHECK(Buffer.CancelSession(Epoch));
	for (int Value = 3; Value < 256; ++Value)
	{
		const auto Invalid = static_cast<EToken>(Value);
		const auto Down = Buffer.KeyDown(Epoch, Invalid);
		const auto Up = Buffer.KeyUp(Epoch, Invalid);
		CHECK(Down.Status == EStatus::StaleSession && Down.bConsumed);
		CHECK(Up.Status == EStatus::StaleSession && Up.bConsumed);
	}
}

int main()
{
	InactiveAndOrder();
	RepeatedAndHeldKeys();
	OldEpochDoesNotReleaseNewKeys();
	FullIsConsumedAndNeverAutoSubmits();
	CancelAndSnapshotIsolation();
	InvalidValuesAreSafe();
	std::cout << "RESULT=MAGIC_COMMAND_BUFFER_PASSED cases=" << Cases << " checks=" << Checks << '\n';
	return 0;
}
