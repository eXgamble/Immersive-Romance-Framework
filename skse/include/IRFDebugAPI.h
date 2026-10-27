#pragma once

#include <cstdint>

// Immersive Romance Framework's debug interface, for test tools.
// ImmersiveRomanceFramework.dll exports IRFGetDebugAPI(); find it with GetModuleHandle + GetProcAddress, so the
// framework runs the same without a debug tool. Every function runs on the game thread only (an SKSE task). New
// functions and fields only ever go at the end, and kVersion goes up: a tool built for version N works with any
// framework of version N or later.
namespace IRFDebug
{
	constexpr std::uint32_t kVersion = 2;
	constexpr std::uint32_t kMaxCircle = 8;

	struct Candidate
	{
		std::uint32_t ref = 0;  // her placed reference
		char          name[64]{};
	};

	struct CircleMember
	{
		std::uint32_t ref = 0;
		char          name[64]{};
		std::int32_t  reaction = 0;        // the reaction waiting for your next meeting (an event; 0: none)
		float         reactionDays = 0.0f;  // days left to hear it
		bool          refusing = false;    // refuses you service
		float         refusalDays = 0.0f;  // days left; 0: until you make amends
		std::int32_t  rankWithPlayer = 0;  // their vanilla relationship rank with you (-1: rival)
	};

	// One candidate as the framework sees her. Stages, moods, events and whys are the framework's numbers (README).
	struct State
	{
		std::int32_t stage = -1;     // -1 none (not a Friend yet), 0 Friend, 2 Dating, 3 Committed, 4 Engaged, 5 Married, 6 Ex, 7 Widowed, 8 Killed
		std::int32_t approval = 0;
		std::int32_t band = 0;       // IRF_BandFaction: approval band 1-5 (her "How are we?")
		std::int32_t mood = 0;       // 0 none, 2 strained, 3 jealous, 4 hurt, 5 wary
		std::int32_t greet = 0;      // IRF_GreetFaction: which greetings are hers now
		bool         following = false;
		bool         hurt = false;   // broken heart: talks, hurt, until amends
		float        silentDays = 0.0f;  // broken heart: won't talk for this long
		std::int32_t who = 0;        // after a breakup: 1 you ended it, 2 she did
		std::int32_t why = 0;        // 1 cheating, 2 violence, 3 absence, 4 bleedouts, 5 crime, 6 choice
		bool         unforgivable = false;
		bool         secret = false;
		float        lockDays = 0.0f;     // after an attack: won't follow or date
		float        waryDays = 0.0f;
		float        jealousDays = 0.0f;
		float        contactDaysAgo = 0.0f;  // since you last spent time together
		std::int32_t recent = 0;             // the last event, and how long ago
		float        recentDaysAgo = 0.0f;
		float        lastDateDaysAgo = 0.0f;
		float        lastKissDaysAgo = 0.0f;
		float        lastGiftDaysAgo = 0.0f;
		std::int32_t pendingBreakup = 0;     // she'll end it at the next talk (a why)
		bool         confrontPending = false;
		std::int32_t knowSeeing = 0;         // how many people know you're together
		std::int32_t sawStraying = 0;        // how many saw you stray
		bool         confessed = false;
		std::int32_t knowAsked = 0;          // the get-to-know questions asked (bit 0 = question 1)
		bool         firstDatePending = false;
		float        dateHoursLeft = 0.0f;   // until the first date is missed
		std::int32_t partnerXp = 0;
		std::int32_t appliedLevels = 0;
		std::int32_t boys = 0;               // adopted children (Hearthfire)
		std::int32_t girls = 0;
		CircleMember circle[kMaxCircle]{};
		std::uint32_t circleCount = 0;
	};

	struct API
	{
		std::uint32_t version = kVersion;
		// Every registered candidate (up to a_max); returns how many there are.
		std::uint32_t (*candidates)(Candidate* a_out, std::uint32_t a_max) = nullptr;
		bool (*read)(std::uint32_t a_ref, State* a_out) = nullptr;
		// Sets her stage as the framework would. Married also puts her in vanilla's PlayerMarriedFaction, and any stage
		// below it takes her out (so a test marriage can be undone); Engaged holds only while vanilla's engagement runs.
		bool (*setStage)(std::uint32_t a_ref, std::int32_t a_stage) = nullptr;
		bool (*setApproval)(std::uint32_t a_ref, std::int32_t a_value) = nullptr;
		// As if it happened: "caught", "bleedout", "attack", "crime", "jealous", "absence", "breakup" (she ends it at
		// the next talk), "dumped" (you end it), "amends", "xp"; "line:<key>" does what her line <key> does when said
		// ("line:kiss.yes", "line:date": straying checks included)
		bool (*deed)(std::uint32_t a_ref, const char* a_deed) = nullptr;
		// Moves her clocks back a_days, as if they had passed (an adopted child's too)
		bool (*skipDays)(std::uint32_t a_ref, std::int32_t a_days) = nullptr;
		// Forgets everything about her: as if you had never met
		bool (*reset)(std::uint32_t a_ref) = nullptr;
		// The child counts as adopted (Hearthfire's Child1/Child2 alias), without Hearthfire's adoption and move
		bool (*adopt)(std::uint32_t a_childRef) = nullptr;
		// A reaction waiting for the next meeting with a_memberRef (a circle member), about a_candidateRef
		bool (*queueReaction)(std::uint32_t a_memberRef, std::uint32_t a_candidateRef, std::int32_t a_event) = nullptr;
		// The first date is on (as after her "askdate.yes")
		bool (*startFirstDate)(std::uint32_t a_ref) = nullptr;
		// The add-on line key of a dialogue line ("hello.dating", "circle.0.reaction.married", ...) and whose it is
		bool (*lineKey)(std::uint32_t a_info, char* a_key, std::uint32_t a_size, std::uint32_t* a_candidateRef) = nullptr;
		// An event's name ("dating", "theyEnded", ...) and back
		bool (*eventName)(std::int32_t a_event, char* a_out, std::uint32_t a_size) = nullptr;
		std::int32_t (*eventByName)(const char* a_name) = nullptr;
		// version 2: runs the jealousy check for her now (past its once-a-day limit), as when you meet her: true with
		// whom and whether a fighter in a_why, or false with why not
		bool (*checkJealous)(std::uint32_t a_ref, char* a_why, std::uint32_t a_size) = nullptr;
	};
}

extern "C" __declspec(dllexport) const IRFDebug::API* IRFGetDebugAPI();
