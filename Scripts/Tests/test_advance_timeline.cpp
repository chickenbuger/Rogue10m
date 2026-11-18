#include "../../Source/Rogue10m/Components/Rogue10mAdvanceTimeline.h"
#include <cstdio>
#include <limits>

#define CHECK(Condition) do { ++Checks; if (!(Condition)) { std::printf("FAIL line %d: %s\n", __LINE__, #Condition); return 1; } } while (false)
static bool Close(double A, double B) { return std::fabs(A - B) < 0.000001; }

int main()
{
	int Checks = 0, Scenarios = 0;
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(!T.Begin(0, .5, .2, 0)); CHECK(!T.Begin(-1, .5, .2, 0));
		CHECK(!T.Begin(601, .5, .2, 0)); CHECK(!T.Begin(100, 0, .2, 0));
		CHECK(!T.Begin(100, .5, -1, 0)); CHECK(!T.Begin(100, 31, .2, 0));
		CHECK(!T.Begin(100, .5, 31, 0));
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		const double Inf = std::numeric_limits<double>::infinity();
		CHECK(!T.Begin(NaN, .5, .2, 0)); CHECK(!T.Begin(100, Inf, .2, 0));
		CHECK(!T.Begin(100, .5, NaN, 0)); CHECK(!T.Begin(100, .5, .2, Inf));
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(150, .5, .25, 8));
		auto A = T.Step(8.25); CHECK(Close(A.Distance, 75)); CHECK(!A.bContactReady);
		auto B = T.Step(8.5); CHECK(Close(B.Distance, 75)); CHECK(B.bContactReady); CHECK(!T.IsActive());
		auto C = T.Step(9); CHECK(Close(C.Distance, 0)); CHECK(!C.bContactReady);
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(100, .25, .75, 0));
		CHECK(!T.Step(.25).bContactReady); CHECK(T.IsActive());
		auto Hold = T.Step(.5); CHECK(Close(Hold.Distance, 0)); CHECK(!Hold.bContactReady);
		CHECK(T.Step(.75).bContactReady);
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(100, 1, .75, 0)); CHECK(Close(T.Step(.25).Distance, 25));
		T.StopTravel(); auto Hold = T.Step(.5); CHECK(Close(Hold.Distance, 0)); CHECK(!Hold.bContactReady);
		auto Contact = T.Step(.75); CHECK(Close(Contact.Distance, 0)); CHECK(Contact.bContactReady);
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(250, .5, .25, 0)); auto Hitch = T.Step(1000);
		CHECK(Close(Hitch.Distance, 250)); CHECK(Hitch.bContactReady); CHECK(Close(T.Step(1001).Distance, 0));
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(100, 1, .5, 1)); CHECK(Close(T.Step(1.25).Distance, 25));
		auto Backward = T.Step(1.125); CHECK(Close(Backward.Distance, 0)); CHECK(!Backward.bContactReady);
		CHECK(Close(T.Step(1.5).Distance, 25));
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(100, 1, .5, 0)); T.Step(.25); T.Cancel();
		auto Late = T.Step(5); CHECK(Close(Late.Distance, 0)); CHECK(!Late.bContactReady);
		CHECK(T.Begin(150, .5, .5, 8)); CHECK(T.Step(8.5).bContactReady);
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(100, 1, .5, 0)); CHECK(!T.Begin(600, .25, 0, 0));
		CHECK(Close(T.Step(.5).Distance, 50));
		auto Invalid = T.Step(std::numeric_limits<double>::quiet_NaN());
		CHECK(!Invalid.bValid); CHECK(!Invalid.bContactReady); CHECK(!T.IsActive());
	}
	{
		++Scenarios;
		FRogue10mAdvanceTimeline T;
		CHECK(T.Begin(150, 1, 1, 0)); T.StopTravel();
		CHECK(!T.Step(.5).bContactReady); auto Contact = T.Step(1);
		CHECK(Contact.bContactReady); CHECK(Close(Contact.Distance, 0)); CHECK(!T.Step(2).bContactReady);
	}
	std::printf("advance_timeline: %d scenarios, %d checks passed; Unreal/physics not run\n", Scenarios, Checks);
	return 0;
}
