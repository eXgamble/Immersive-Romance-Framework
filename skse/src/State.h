#pragma once

#include "Types.h"

// What the framework remembers beyond faction ranks, saved in the SKSE co-save as tagged fields (State.cpp), so
// saves survive updates. A new saved value: a member here with its default, and one line with a new tag in its
// Fields list in State.cpp (a value only a session needs stays out of that list). Days are GameDaysPassed.
// Everything is reset on Revert and read back on load.
namespace State
{
	inline constexpr float kNever = -1.0e9f;  // "no such day yet" (game days can be negative in debug use)

	struct Candidate
	{
		float lastContactDay{ kNever };  // the last time you spent time together (talk, date, following)
		float silentUntilDay{ 0.0f };    // broken heart: won't talk until this day
		bool  hurt{ false };             // broken heart: talks, hurt, until amends
		IRF::Why  why{ IRF::Why::kNone };
		IRF::Who  who{ IRF::Who::kNone };
		float breakupDay{ 0.0f };
		bool  unforgivable{ false };     // a "never" tolerance was crossed: no amends, no win-back
		bool  secret{ false };
		float lockUntilDay{ 0.0f };      // after an attack: won't follow or date until this day
		float lastDateDay{ -10.0f };
		float lastKissDay{ -10.0f };
		float lastGiftDay{ -10.0f };     // one gift a day: "I have something for you." waits until a day has passed
		float killDay{ -10.0f };
		int   killsToday{ 0 };
		int   strayHits{ 0 };            // in the current fight
		IRF::Event recent{ IRF::Event::kNone };
		float recentDay{ 0.0f };
		int   partnerXp{ 0 };
		int   appliedLevels{ 0 };        // partner growth already applied to their actor values
		bool  profileApplied{ false };   // the combat profile is on (they follow)
		float savedConfidence{ -1.0f };
		float savedAggression{ -1.0f };
		IRF::Why pendingBreakup{ IRF::Why::kNone };  // they'll end it at the next talk
		std::set<RE::FormID> knowSeeing;    // who knows you're with this candidate
		std::set<RE::FormID> sawStraying;   // who saw you stray from this candidate (word may get back)
		bool  confrontPending{ false };
		bool  confrontWorks{ false };
		float waryUntilDay{ 0.0f };      // her moods (Mood) that last a while
		float jealousUntilDay{ 0.0f };
		int   pleadsLeft{ 0 };           // a pleading ex you left: times she'll still ask you back ...
		float nextPleadDay{ 0.0f };      // ... from this day
		float datingDay{ kNever };       // when she last became Dating (the first date, a win-back; not a drop from Committed)
		float readyDay{ kNever };        // Dating, approval reached the commitment bar this day: Committed once she tells you (Confess)
		bool  confessed{ false };        // she told you how much you mean to her (once per relationship)
		std::uint16_t knowAsked{ 0 };    // the get-to-know questions you asked (bit 0 = question 1)
		float lastKnowDay{ -10.0f };     // the last day you asked one you hadn't (one a day)
		float dateUntilDay{ 0.0f };      // the first date is set: come by this day (the end of that evening); 0 none
	};

	// A reaction waiting for the next time you meet someone
	struct Reaction
	{
		IRF::Event event{ IRF::Event::kNone };
		float      untilDay{ 0.0f };
	};

	// A circle member refusing service
	struct Refusal
	{
		RE::FormID because{ 0 };  // the candidate it's about
		float      untilDay{ 0.0f };  // 0 = until amends with that candidate
	};

	// An adopted child (Hearthfire): when you last saw them, when they last told you something was wrong at home
	struct Child
	{
		float lastSeenDay{ kNever };  // kNever: not met since adoption; the first meeting sets it
		float lastMoodDay{ -10.0f };  // a line about the marriage, once a day
	};

	Candidate&                                  Of(RE::FormID a_candidate);
	std::unordered_map<RE::FormID, Child>&      Children();  // by the child's reference
	std::unordered_map<RE::FormID, Candidate>&  All();
	std::unordered_map<RE::FormID, Reaction>&   Reactions();  // by the reacting actor's reference
	std::unordered_map<RE::FormID, Refusal>&    Refusals();   // by the refusing actor's reference
	std::string&                                PlayerInterest();  // "", "any", "male", "female" (Papyrus override of the INI)

	float Today();

	void Register();  // SKSE serialization callbacks
}
