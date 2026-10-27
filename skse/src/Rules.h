#pragma once

// The numbers, in one place: Data/SKSE/Plugins/ImmersiveRomanceFramework/Rules.json overrides any of them, so they
// can be tuned in play without a rebuild. Every value here is a placeholder until balanced (approval 0..100).
namespace Rules
{
	struct Values
	{
		// systems on or off (Rules.json "systems" section): off, the framework leaves that part of the game alone
		bool jealousy = true;        // she gets jealous of another woman at your side
		bool crimes = true;          // crimes she sees cost (or earn) approval, with her line
		bool murderLeaves = true;    // a good-hearted one leaves you over a murder (murderLeaveAlignment)
		bool absence = true;         // a long time apart costs approval
		bool bleedouts = true;       // going down at your side costs approval
		bool exposeScenes = true;    // two partners have it out (off: both simply catch you)
		bool familyTalks = true;     // she and an adopted child talk at home
		bool stayHome = true;        // a wife with children won't leave town with you
		bool confession = true;      // at the commitment bar she comes to tell you how she feels, and becomes Committed (off: at once)
		bool serviceRefusal = true;  // her family or friends refuse you their shop or training over a broken heart
		bool roadLines = true;       // her lines on the road (places, after a fight, dragons, resting)

		int startApproval = 20;   // when someone becomes a Friend
		int askDateApproval = 35; // a Friend says yes to "Would you spend an evening with me?" from this much
		int standUpCost = 10;     // you didn't come to the first date
		float firstDateDistance = 1000.0f;  // at her spot and this close, after sunset: she comes over
		int commitBar = 75;       // Dating -> Committed from this (once she tells you, Confess)
		int uncommitBar = 60;     // Committed -> back to Dating below this (leeway: one bad fight doesn't undo it)
		int floorDating = 1;      // at or below 0 from Dating up: they end it (approval ran out)

		int knowGain = 3;       // a question you hadn't asked her yet (one a day)
		int dateGain = 3;       // "Spend the evening with me." (18:00-23:30, from Dating up, every dateEveryDays)
		float dateEveryDays = 2.0f;  // an evening together is offered this often (the road should be where approval grows)
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
		int cheatCost = 30;         // default cheat-tolerance
		int brokenHeartDays = 3;    // default
		int hitCost = 2;            // default hit-tolerance: a stray hit
		int strayHitsToAttack = 4;  // stray hits in one fight that count as an attack
		int attackCost = 25;        // an attack
		int attackLockDays = 3;     // won't follow or date after an attack
		int absenceDays = 7;        // default absence-tolerance
		int absenceCost = 5;        // per band, once when you come back
		int jealousCost = 2;        // seen travelling with another woman, once a day
		int waryDays = 5;           // default wary-tolerance: days she's wary of you after an attack
		int jealousDays = 1;        // her jealous mood after seeing you with another woman: the rest of that day (1) or more
		// approval bands ("How are we?", how things are at home: 4 and 5 happy, 3 fine, 2 strained, 1 bad): where each starts.
		// Below band3 she's strained (her mood); below band2 things are bad and her spouse perk pauses
		int band2 = 25;
		int band3 = 50;
		int band4 = 70;
		int band5 = 85;
		int heartbreakSilentDays = 5;  // a bitter candidate you left (or who caught you) won't talk this long
		int pleads = 3;             // a pleading candidate you left asks you back this many times ...
		int pleadEvery = 2;         // ... this many days apart
		int crimeScale = 1;         // approval per alignment point per crime severity point
		int murderLeaveAlignment = 2;  // a murder she sees, at this alignment or higher: wary, and she leaves you and goes home
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

		float confessDelayDays = 1.0f;        // at the commitment bar, she tells you how much you mean to her after this long
		float confessMinDatingDays = 3.0f;    // ... and not before you've been Dating this long (a candidate's confession.minDatingDays overrides it)
		float confessDistance = 3000.0f;      // ... when you're this close (she walks over)
		int   childAwayDays = 5;              // an adopted child misses you after this many days apart (2x, 3x: more)
		float momentCooldownSeconds = 90.0f;  // between two road lines
		float witnessDistance = 2048.0f;      // ~30 m: farther than this, nobody "saw" it
		float caughtDistance = 2048.0f;
		float jealousDistance = 1536.0f;
		float exposeDistance = 1024.0f;  // ~15 m: a partner this close hears another greet you as hers
		float caughtLineGap = 4.0f;      // seconds between two partners' caught lines in one scene
		float exposeLeaveDistance = 4096.0f;  // walk this far from them during the scene and it's broken off
		int   exposePersuade = 50;            // Speech that carries the lie when the two have it out (fixed)
		int   exposeDoubtCost = 5;            // approval the one who believed you still loses
		int   disownedSpouseMult = 3;         // a wife you denied: her broken heart lasts this many times as long
		float familyHearDistance = 1500.0f;   // a family conversation: she and a child this close to each other and to you
	};

	// Rules.json, then Rules.user.json over it (only the keys it gives: a modlist's own changes, kept apart from the
	// framework's defaults so an update's new defaults still come through)
	void          Load();
	const Values& Get();
	// One rule set by name, as a JSON value ("false", "12"): IRF - Debug's SetRule, for tests. False: no such rule
	bool Set(std::string_view a_name, std::string_view a_json);

	// Who the player is interested in: "any", "male" or "female" (Rules.json playerInterestedIn; eXrim's
	// character creator overrides it through IRF.SetPlayerInterest)
	const std::string& PlayerInterest();
}
