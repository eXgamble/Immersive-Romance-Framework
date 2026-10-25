#pragma once

// The codes the framework writes into faction ranks, so lines can be conditioned on them. They are part of the
// add-on contract: never renumber, only add.
namespace IRF
{
	enum class Stage : std::int8_t
	{
		kNone = -1,  // not a Friend yet (vanilla relationship rank below 1, or not someone they'd court)
		kFriend = 0,
		kCourting = 1,
		kDating = 2,
		kCommitted = 3,
		kEngaged = 4,
		kMarried = 5,
		kEx = 6,
		kWidowed = 7,  // the candidate died; the player is their widow(er)
		kKilled = 8,   // the candidate died at the player's hand and someone saw it (no widow(er) of your own victim)
	};

	// IRF_GreetFaction: which hellos and goodbyes are theirs right now (0 = none, vanilla greets them)
	enum class GreetState : std::int8_t
	{
		kNone = 0,
		kCourting = 1,  // retired: Courting greets as vanilla's Friend (kNone)
		kDating = 2,
		kCommitted = 3,
		kEngaged = 4,
		kSpouse = 5,
		kBrokenSilent = 10,  // won't talk at all: the hello carries the Goodbye flag
		kBrokenHurt = 11,    // talks, hurt, until amends
		kEx = 20,            // + Why: an ex greets you by why it ended (21 cheating .. 26 choice)
		kPlead = 30,         // a pleading ex you left asks you to take her back (her heartbreak style)
	};

	// Why a relationship ended (IRF_WhyFaction), and what drove an approval drop
	enum class Why : std::int8_t
	{
		kNone = 0,
		kCheating = 1,
		kViolence = 2,
		kAbsence = 3,
		kBleedouts = 4,
		kCrime = 5,
		kChoice = 6,  // the player ended it
	};

	// Who ended it (IRF_WhoFaction)
	enum class Who : std::int8_t
	{
		kNone = 0,
		kPlayer = 1,
		kThem = 2,
	};

	// Events: a pending reaction (IRF_ReactionFaction) and the recent event (IRF_RecentFaction) use these
	enum class Event : std::int8_t
	{
		kNone = 0,
		kCourting = 1,
		kDating = 2,
		kCommitted = 3,
		kEngaged = 4,
		kMarried = 5,
		kAmends = 6,
		kPlayerEnded = 7,       // the player broke it off
		kTheyEnded = 8,         // the candidate left the player
		kBrokenHeart = 9,       // the player broke their heart (caught)
		kSeeingSomeone = 10,    // the player is dating someone else (said by other candidates who know)
		kWidowed = 11,          // the candidate died
		kWonBack = 12,          // back together after a breakup
		kKilled = 13,           // the player killed them, and someone saw it
		kBreakupSpeech = 20,    // + Why: the candidate's own speech as she leaves you (21 cheating .. 25 crime)
		kConfront = 30,         // word got back: she confronts you
	};

	// IRF_MomentFaction: what a line said through IRF_SayTopic is about
	enum class Moment : std::int8_t
	{
		kNone = 0,
		kCaught = 1,           // she caught you with someone else
		kRevive = 2,           // you got her back up quickly
		kAttacked = 3,         // you attacked her
		kCrimeApprove = 4,     // she saw a crime and liked it (alignment below 0)
		kCrimeDisapprove = 5,  // she saw a crime and didn't (alignment above 0)
		kJealous = 6,          // you travel with another candidate
		kRefuse = 7,           // a circle member refuses you service
		kStayHome = 8,         // a spouse with children won't leave town (her force greet at the town's edge)
		kAfterFight = 10,
		kDragon = 11,
		kNearlyDead = 12,
		kRest = 13,
		kNordicRuin = 20,
		kDwarvenRuin = 21,
		kCave = 22,
		kDungeon = 23,
		kTown = 24,
		kInn = 25,
		kHomeTown = 26,
	};

	// IRF_MoodFaction: her mood on top of the stage (Courting to Married), for the mood versions of her greetings,
	// goodbyes and idles; the strongest wins. No mood is the happy default: her plain stage lines (a spouse's idles
	// mix with vanilla's). 1 was warm, removed (user, 2026-10-25): not reused
	enum class Mood : std::int8_t
	{
		kNone = 0,
		kStrained = 2,  // approval low
		kJealous = 3,   // she saw you travelling with another candidate lately
		kHurt = 4,      // broken heart (idles only: her greetings already have their own)
		kWary = 5,      // you attacked her lately (how she takes it: her add-on's wary style)
	};

	// IRF_ChildFaction: what an adopted child's greeting is about (the framework's own lines)
	enum class ChildMood : std::int8_t
	{
		kNone = 0,          // all is well: vanilla's greeting
		kAway1 = 1,         // you were away a while .. 3 a very long time
		kAway2 = 2,
		kAway3 = 3,
		kStrained = 11,     // the marriage is strained (spouse approval band 2)
		kBad = 12,          // the marriage is in a bad way (band 1)
		kHeartbroken = 13,  // the spouse is heartbroken (caught you)
	};

	// How a candidate takes a gift; IRF_GiftFaction carries it (+ 1) for her answer, her goodbye
	enum class Taste : std::int8_t
	{
		kInsulted = 0,
		kDisliked = 1,
		kNeutral = 2,
		kLiked = 3,
		kLoved = 4,
	};

	inline int Rank(auto a_enum) { return static_cast<int>(a_enum); }
}
