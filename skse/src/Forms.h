#pragma once

// The framework's own records (Immersive Romance Framework.esp) and the vanilla ones it touches. FormIDs and
// EditorIDs of the framework's records are frozen from 1.0: add-ons put lines into its topics and their voice
// file paths contain these names.
namespace Forms
{
	inline constexpr auto kPlugin = "Immersive Romance Framework.esp"sv;

	enum Local : RE::FormID
	{
		// facts lines read (factions, ranks on the speaker)
		kCandidateFaction = 0x800,  // every registered candidate, rank 0
		kStageFaction = 0x801,      // rank = Stage; not a member before Friend
		kApprovalFaction = 0x802,   // rank = approval 0..100
		kGreetFaction = 0x803,      // rank = GreetState, what the hellos and goodbyes read
		kQuestBusyFaction = 0x804,  // rank 1 while a running story quest holds them: romance dialogue stands aside
		kAmuletFaction = 0x80C,     // rank = stage + 1 while vanilla would offer the amulet line (below Committed)
		kBandFaction = 0x80F,       // approval band 1..5, for "How are we?"
		kWhyFaction = 0x810,        // last breakup's why (Why)
		kWhoFaction = 0x811,        // last breakup's who (1 player, 2 them)
		kAbsenceFaction = 0x812,    // pending absence hello band 1..3
		kReactionFaction = 0x813,   // pending reaction code (Reaction), on anyone who reacts
		kRecentFaction = 0x814,     // the last event with this candidate (Reaction code)
		kRecentDaysFaction = 0x815, // days since that event, capped at 100
		kMomentFaction = 0x816,     // what a line said through IRF_SayTopic is about (Moment)
		kRefuseFaction = 0x817,     // refuses the player service (circle consequences)
		kAskDateFaction = 0x818,    // answer to "Would you be mine?": 1 yes, 2 not yet
		kFollowFaction = 0x819,     // answer to "Come with me": 1 yes, 2 no
		kKissFaction = 0x81A,       // answer to a kiss: 1 yes, 2 not here, 3 no
		kBreakUpFaction = 0x81B,    // 1 while "I think we should end this" applies
		kAmendsFaction = 0x81C,     // 1 while amends can be made
		kWinBackFaction = 0x81D,    // answer to "Could we try again?": 1 yes, 2 no
		kSecretFaction = 0x81E,     // answer to "Let's keep this between us": 1 yes, 2 no
		kDateFaction = 0x81F,       // 1 while "Spend the evening with me." applies (evening, Dating up, once a day)
		kConfrontFaction = 0x820,   // word got back: 1 your denial will work, 2 it won't
		kCourtFaction = 0x821,      // "I'd like to get to know you better.": 1 she answers (once a day)

		// items
		kGiftInsultingKeyword = 0x83F,  // IRF_GiftInsulting: an item the average person takes as an insult

		// gifts: "I have something for you." opens the gift menu, her answer is her goodbye
		kGiveFaction = 0x840,      // 1 while she'd take a gift (the line is offered)
		kGiftFaction = 0x841,      // her answer to the gift just given: Taste + 1 (1 insulted .. 5 loved), 0 otherwise
		kGiveTopic = 0x842,        // IRF_GiveTopic (in IRF_Quest), its blank line opens the gift menu
		kGiftQuest = 0x845,        // IRF_GiftQuest, priority 110: above every quest's goodbyes
		kGiftAnswerTopic = 0x846,  // IRF_GiftAnswerTopic, a goodbye topic

		// adopted children (Hearthfire): their greetings, and a spouse who stays home with them
		kChildFaction = 0x847,       // rank = ChildMood, what an adopted child's greeting is about (0 = vanilla's)
		kForceGreetTopic = 0x848,    // IRF_ForceGreetTopic: what she comes up to tell you (IRF_MomentFaction: stay home, confess)
		kForceGreetPackage = 0x84A,  // IRF_ForceGreet, on IRF_SayQuest's ForceGreet alias
		kConfessReplyTopic = 0x862,  // IRF_ConfessReplyTopic: "You mean a lot to me too." (linked from her confession)
		kChildQuest = 0x851,         // IRF_ChildQuest, priority 105, speaks only for a child with something to say
		kChildHelloTopic = 0x852,    // IRF_ChildHelloTopic: the children's greetings (IRF_ChildFaction 1-3, 11-13)
		kAdoptedBoys = 0x859,        // IRF_AdoptedBoys: how many boys Hearthfire has you raising (for any line's conditions)
		kAdoptedGirls = 0x85A,       // IRF_AdoptedGirls: and girls

		// her mood and her idle chatter
		kMoodFaction = 0x85B,        // rank = Mood
		kIdleTopic = 0x85C,          // IRF_IdleTopic (in IRF_Quest): her idles by stage (IRF_StageFaction) and mood

		// a pleading ex: "Alright. Let's try again." / "No. It's over."
		kPleadFaction = 0x85D,       // 1 while she's asking you to take her back
		kSpouseBonusFaction = 0x863, // 1 while her spouse perk is on (married, in good standing)
		kChatFaction = 0x86E,        // 1 while she may chat on the road: an add-on's own follower comments read it
		kFirstDateQuest = 0x86F,     // IRF_FirstDateQuest: her (alias 0, IRF_FirstDateWait 0x870), her spot (1), its place (2)
		kFirstDateReplyTopic = 0x871, // IRF_FirstDateReplyTopic: "Of course I came." (linked from her first-date greeting)
		kCircleHelloTopic = 0x873,    // IRF_CircleHelloTopic / GoodbyeTopic (IRF_CircleQuest 0x872, open to any speaker):
		kCircleGoodbyeTopic = 0x874,  // a circle member's everyday greetings about the two of you
		kReplyFaction = 0x875,        // IRF_ReplyFaction: which reply she's saying (moment kReply)
		kPleadYesTopic = 0x85E,
		kPleadNoTopic = 0x860,

		// quests and topics
		kQuest = 0x805,  // IRF_Quest, priority 105
		kHelloTopic = 0x806,
		kGoodbyeTopic = 0x807,
		kCourtTopic = 0x808,
		kHowAreWeTopic = 0x80A,
		kAmuletTopic = 0x80D,
		kReactionQuest = 0x822,  // priority 108
		kReactionHelloTopic = 0x823,
		kAbsenceQuest = 0x824,  // priority 107
		kAbsenceHelloTopic = 0x825,
		kRecentQuest = 0x826,  // priority 106
		kRecentHelloTopic = 0x827,
		kSayQuest = 0x828,
		kSayTopic = 0x829,
		kAskDateTopic = 0x82A,
		kFollowTopic = 0x82C,
		kKissTopic = 0x82E,
		kBreakUpTopic = 0x830,
		kAmendsTopic = 0x832,
		kWinBackTopic = 0x834,
		kSecretTopic = 0x836,
		kDateTopic = 0x838,
		kKnowTopic1 = 0x864,         // IRF_KnowTopic1-10 (0x864-0x86D): the questions under "I'd like to get to know you better."
		kDenyTopic = 0x83A,
		kAdmitTopic = 0x83C,
	};

	struct Set
	{
		RE::TESFaction* candidate{ nullptr };
		RE::TESFaction* stage{ nullptr };
		RE::TESFaction* approval{ nullptr };
		RE::TESFaction* greet{ nullptr };
		RE::TESFaction* questBusy{ nullptr };
		RE::TESFaction* amulet{ nullptr };
		RE::TESFaction* band{ nullptr };
		RE::TESFaction* why{ nullptr };
		RE::TESFaction* who{ nullptr };
		RE::TESFaction* absence{ nullptr };
		RE::TESFaction* reaction{ nullptr };
		RE::TESFaction* recent{ nullptr };
		RE::TESFaction* recentDays{ nullptr };
		RE::TESFaction* moment{ nullptr };
		RE::TESFaction* reply{ nullptr };
		RE::TESFaction* refuse{ nullptr };
		RE::TESFaction* askDate{ nullptr };
		RE::TESFaction* follow{ nullptr };
		RE::TESFaction* kiss{ nullptr };
		RE::TESFaction* breakUp{ nullptr };
		RE::TESFaction* amends{ nullptr };
		RE::TESFaction* winBack{ nullptr };
		RE::TESFaction* secret{ nullptr };
		RE::TESFaction* date{ nullptr };
		RE::TESFaction* confront{ nullptr };
		RE::TESFaction* courtFaction{ nullptr };

		std::vector<RE::TESQuest*> quests;  // every framework quest, started at load if a save lacks one

		RE::TESTopic* hello{ nullptr };
		RE::TESTopic* goodbye{ nullptr };
		RE::TESTopic* court{ nullptr };
		RE::TESTopic* howAreWe{ nullptr };
		RE::TESTopic* amuletTopic{ nullptr };
		RE::TESTopic* reactionHello{ nullptr };
		RE::TESTopic* absenceHello{ nullptr };
		RE::TESTopic* recentHello{ nullptr };
		RE::TESTopic* say{ nullptr };
		RE::TESTopic* askDateTopic{ nullptr };
		RE::TESTopic* followTopic{ nullptr };
		RE::TESTopic* kissTopic{ nullptr };
		RE::TESTopic* breakUpTopic{ nullptr };
		RE::TESTopic* amendsTopic{ nullptr };
		RE::TESTopic* winBackTopic{ nullptr };
		RE::TESTopic* secretTopic{ nullptr };
		RE::TESTopic* dateTopic{ nullptr };
		std::array<RE::TESTopic*, 10> know{};  // IRF_KnowTopic1-10
		RE::TESTopic* denyTopic{ nullptr };
		RE::TESTopic* admitTopic{ nullptr };
		RE::BGSKeyword* giftInsulting{ nullptr };
		RE::TESFaction* give{ nullptr };
		RE::TESFaction* gift{ nullptr };
		RE::TESTopic*   giveTopic{ nullptr };
		RE::TESTopic*   giftAnswer{ nullptr };
		RE::TESFaction* child{ nullptr };
		RE::TESTopic*   childHello{ nullptr };
		RE::TESGlobal*  adoptedBoys{ nullptr };
		RE::TESGlobal*  adoptedGirls{ nullptr };
		RE::TESFaction* moodFaction{ nullptr };
		RE::TESTopic*   idle{ nullptr };
		RE::TESFaction* plead{ nullptr };
		RE::TESFaction* spouseBonus{ nullptr };
		RE::TESFaction* chat{ nullptr };
		RE::TESQuest*   firstDateQuest{ nullptr };
		RE::TESTopic*   firstDateReply{ nullptr };
		RE::TESTopic*   circleHello{ nullptr };
		RE::TESTopic*   circleGoodbye{ nullptr };
		RE::TESTopic*   pleadYes{ nullptr };
		RE::TESTopic*   pleadNo{ nullptr };
		RE::TESTopic*   forceGreetTopic{ nullptr };
		RE::TESTopic*   confessReply{ nullptr };
		RE::TESQuest*   sayQuest{ nullptr };     // IRF_SayQuest: its alias 0, ForceGreet, holds the force greet

		// Hearthfire (looked up softly: without it there are no adopted children)
		RE::TESQuest*   adoption{ nullptr };         // BYOHRelationshipAdoption 0042B4
		RE::TESFaction* adoptionFaction{ nullptr };  // BYOHRelationshipAdoptionFaction 0042B0, every adopted child

		// vanilla
		RE::TESFaction*    potentialMarriage{ nullptr };  // PotentialMarriageFaction 019809
		RE::TESFaction*    playerMarried{ nullptr };      // PlayerMarriedFaction 0C6472
		RE::TESFaction*    currentFollower{ nullptr };    // CurrentFollowerFaction 05C84E
		RE::TESFaction*    brawl{ nullptr };              // DGIntimidateFaction 04CFA6
		RE::TESQuest*      marriage{ nullptr };           // RelationshipMarriage 074793
		RE::TESQuest*      wedding{ nullptr };            // RelationshipMarriageWedding 07404E
		RE::TESQuest*      followerQuest{ nullptr };      // DialogueFollower 0750BA
		RE::TESObjectARMO* amuletOfMara{ nullptr };       // ReligiousSymbolMaraLove 0C891B
		RE::SpellItem*     loversComfort{ nullptr };      // MarriageRested 0CDA1D
		RE::TESGlobal*     speechVeryHard{ nullptr };     // SpeechVeryHard 0D1954
		RE::BGSKeyword*    dragon{ nullptr };             // ActorTypeDragon 035D59
		RE::BGSKeyword*    locPlayerHouse{ nullptr };     // LocTypePlayerHouse 0FC1A3
		RE::BGSKeyword*    locHouse{ nullptr };           // LocTypeHouse 01CB85
		RE::BGSKeyword*    locDwelling{ nullptr };        // LocTypeDwelling 0130DC
		RE::BGSKeyword*    locInn{ nullptr };             // LocTypeInn 01CB87
		RE::BGSKeyword*    locCity{ nullptr };            // LocTypeCity 013168
		RE::BGSKeyword*    locTown{ nullptr };            // LocTypeTown 013166
		RE::BGSKeyword*    locSettlement{ nullptr };      // LocTypeSettlement 013167
		RE::BGSKeyword*    locHabitation{ nullptr };      // LocTypeHabitation 039793
		RE::BGSKeyword*    locDungeon{ nullptr };         // LocTypeDungeon 0130DB
		RE::BGSKeyword*    locNordicRuin{ nullptr };      // LocSetNordicRuin 0130F2
		RE::BGSKeyword*    locDwarvenRuin{ nullptr };     // LocSetDwarvenRuin 0130F0
		RE::BGSKeyword*    locCave{ nullptr };            // LocSetCave 0130EF
	};

	// Looks everything up once at kDataLoaded; false when the plugin is missing or a record is
	bool Load();
	const Set& Get();
}
