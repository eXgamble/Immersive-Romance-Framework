#pragma once

// The numbers, in one place: Data/SKSE/Plugins/ImmersiveRomanceFramework/Rules.json overrides any of them, so they
// can be tuned in play without a rebuild. Every value here is a placeholder until balanced (approval 0..100).
namespace Rules
{
	struct Values
	{
		int startApproval = 20;   // when someone becomes a Friend
		int askDateApproval = 35; // Courting -> Dating needs this much
		int commitBar = 60;       // Dating -> Committed at this; below it again -> back to Dating
		int floorDating = 1;      // at or below 0 from Dating up: they end it (approval ran out)

		int talkGain = 2;       // once a day
		int dateGain = 4;       // a talk in the evening (18:00-23:00) counts as a date, once a day
		int kissGain = 1;       // once a day
		int giftGain = 3;          // a gift (one a day)
		int giftLikedGain = 5;     // a gift they like (instead)
		int giftLovedGain = 8;     // a gift they love (instead)
		int giftDislikedCost = 3;  // a gift they dislike
		int giftInsultCost = 15;   // a gift they take as an insult (not taken)
		int giftGoldLiked = 10;    // gold: below this they dislike it, from this they like it
		int giftGoldLoved = 300;   // gold: from this they love it
		int killGain = 1;       // an enemy killed while they follow, up to killCapPerDay a day
		int killCapPerDay = 5;
		int clearGain = 5;      // a location cleared together

		int bleedoutCost = 8;       // default bleedout-tolerance (what one bleedout costs)
		int reviveBack = 4;         // won back for a quick revive
		float reviveSeconds = 30.0f;// "quick"
		int cheatCost = 30;         // default cheat-tolerance
		int brokenHeartDays = 3;    // default
		int hitCost = 2;            // default hit-tolerance: a stray hit
		int strayHitsToAttack = 4;  // stray hits in one fight that count as an attack
		int attackCost = 25;        // an attack
		int attackLockDays = 3;     // won't follow or date after an attack
		int absenceDays = 7;        // default absence-tolerance
		int absenceCost = 5;        // per band, once when you come back
		int jealousCost = 3;        // travelling with another candidate
		int waryDays = 5;           // default wary-tolerance: days she's wary of you after an attack
		int jealousDays = 3;        // her jealous mood after seeing you with another candidate
		int warmDays = 2;           // her warm mood after something good (a quick revive, amends, the wedding, a win-back)
		int strainedBelow = 15;     // approval under this: strained mood
		int warmFrom = 80;          // approval from this: warm mood
		int crimeScale = 1;         // approval per alignment point per crime severity point
		int winBackDays = 7;
		int winBackApproval = 15;   // approval they start from when they take you back
		int amendsApproval = 0;     // added when amends are made
		int reactionDays = 7;       // a pending reaction expires after this
		int garrulousScale = 100;   // garrulous 0..100 is the chance in percent

		int partnerXpPerLevel = 100;  // partner growth: xp per level
		int partnerXpKill = 10;       // xp for an enemy killed while following (times its level / 10, at least 1)
		int partnerXpClear = 50;      // xp for a location cleared together
		int partnerHealthPerLevel = 10;
		int partnerSkillPerLevel = 2;

		int   childAwayDays = 5;              // an adopted child misses you after this many days apart (2x, 3x: more)
		float momentCooldownSeconds = 90.0f;  // between two road lines
		float witnessDistance = 2048.0f;      // ~30 m: farther than this, nobody "saw" it
		float caughtDistance = 2048.0f;
		float jealousDistance = 1536.0f;
	};

	void          Load();
	const Values& Get();

	// Who the player is interested in: "any", "male" or "female" (Rules.json playerInterestedIn; eXrim's
	// character creator overrides it through IRF.SetPlayerInterest)
	const std::string& PlayerInterest();
}
