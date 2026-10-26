#include "Forms.h"

namespace Forms
{
	namespace
	{
		Set g_set;

		struct Finder
		{
			RE::TESDataHandler* data;
			bool                ok{ true };

			template <class T>
			T* Own(RE::FormID a_local, const char* a_what)
			{
				return Get<T>(a_local, kPlugin, a_what);
			}

			template <class T>
			T* Vanilla(RE::FormID a_local, const char* a_what)
			{
				return Get<T>(a_local, "Skyrim.esm"sv, a_what);
			}

			template <class T>
			T* Get(RE::FormID a_local, std::string_view a_plugin, const char* a_what)
			{
				auto form = data->LookupForm<T>(a_local, a_plugin);
				if (!form) {
					logger::error("Forms: {} ({:06X} in {}) not found", a_what, a_local, a_plugin);
					ok = false;
				}
				return form;
			}
		};
	}

	bool Load()
	{
		auto data = RE::TESDataHandler::GetSingleton();
		if (!data || !data->LookupModByName(kPlugin)) {
			logger::error("Forms: {} is not loaded; the framework stays off", kPlugin);
			return false;
		}
		Finder f{ data };
		auto&  s = g_set;

		s.candidate = f.Own<RE::TESFaction>(kCandidateFaction, "IRF_CandidateFaction");
		s.stage = f.Own<RE::TESFaction>(kStageFaction, "IRF_StageFaction");
		s.approval = f.Own<RE::TESFaction>(kApprovalFaction, "IRF_ApprovalFaction");
		s.greet = f.Own<RE::TESFaction>(kGreetFaction, "IRF_GreetFaction");
		s.questBusy = f.Own<RE::TESFaction>(kQuestBusyFaction, "IRF_QuestBusyFaction");
		s.amulet = f.Own<RE::TESFaction>(kAmuletFaction, "IRF_AmuletFaction");
		s.band = f.Own<RE::TESFaction>(kBandFaction, "IRF_BandFaction");
		s.why = f.Own<RE::TESFaction>(kWhyFaction, "IRF_WhyFaction");
		s.who = f.Own<RE::TESFaction>(kWhoFaction, "IRF_WhoFaction");
		s.absence = f.Own<RE::TESFaction>(kAbsenceFaction, "IRF_AbsenceFaction");
		s.reaction = f.Own<RE::TESFaction>(kReactionFaction, "IRF_ReactionFaction");
		s.recent = f.Own<RE::TESFaction>(kRecentFaction, "IRF_RecentFaction");
		s.recentDays = f.Own<RE::TESFaction>(kRecentDaysFaction, "IRF_RecentDaysFaction");
		s.moment = f.Own<RE::TESFaction>(kMomentFaction, "IRF_MomentFaction");
		s.refuse = f.Own<RE::TESFaction>(kRefuseFaction, "IRF_RefuseFaction");
		s.askDate = f.Own<RE::TESFaction>(kAskDateFaction, "IRF_AskDateFaction");
		s.follow = f.Own<RE::TESFaction>(kFollowFaction, "IRF_FollowAnswerFaction");
		s.kiss = f.Own<RE::TESFaction>(kKissFaction, "IRF_KissFaction");
		s.breakUp = f.Own<RE::TESFaction>(kBreakUpFaction, "IRF_BreakUpFaction");
		s.amends = f.Own<RE::TESFaction>(kAmendsFaction, "IRF_AmendsFaction");
		s.winBack = f.Own<RE::TESFaction>(kWinBackFaction, "IRF_WinBackFaction");
		s.secret = f.Own<RE::TESFaction>(kSecretFaction, "IRF_SecretFaction");
		s.date = f.Own<RE::TESFaction>(kDateFaction, "IRF_DateFaction");
		s.confront = f.Own<RE::TESFaction>(kConfrontFaction, "IRF_ConfrontFaction");
		s.courtFaction = f.Own<RE::TESFaction>(kCourtFaction, "IRF_CourtFaction");

		s.quests.clear();
		for (const auto id : { kQuest, kReactionQuest, kAbsenceQuest, kRecentQuest, kSayQuest, kGiftQuest, kChildQuest }) {
			if (const auto quest = f.Own<RE::TESQuest>(id, "framework quest")) {
				s.quests.push_back(quest);
			}
		}

		s.hello = f.Own<RE::TESTopic>(kHelloTopic, "IRF_HelloTopic");
		s.goodbye = f.Own<RE::TESTopic>(kGoodbyeTopic, "IRF_GoodbyeTopic");
		s.court = f.Own<RE::TESTopic>(kCourtTopic, "IRF_CourtTopic");
		s.howAreWe = f.Own<RE::TESTopic>(kHowAreWeTopic, "IRF_HowAreWeTopic");
		s.amuletTopic = f.Own<RE::TESTopic>(kAmuletTopic, "IRF_AmuletTopic");
		s.reactionHello = f.Own<RE::TESTopic>(kReactionHelloTopic, "IRF_ReactionHelloTopic");
		s.absenceHello = f.Own<RE::TESTopic>(kAbsenceHelloTopic, "IRF_AbsenceHelloTopic");
		s.recentHello = f.Own<RE::TESTopic>(kRecentHelloTopic, "IRF_RecentHelloTopic");
		s.say = f.Own<RE::TESTopic>(kSayTopic, "IRF_SayTopic");
		s.askDateTopic = f.Own<RE::TESTopic>(kAskDateTopic, "IRF_AskDateTopic");
		s.followTopic = f.Own<RE::TESTopic>(kFollowTopic, "IRF_FollowTopic");
		s.kissTopic = f.Own<RE::TESTopic>(kKissTopic, "IRF_KissTopic");
		s.breakUpTopic = f.Own<RE::TESTopic>(kBreakUpTopic, "IRF_BreakUpTopic");
		s.amendsTopic = f.Own<RE::TESTopic>(kAmendsTopic, "IRF_AmendsTopic");
		s.winBackTopic = f.Own<RE::TESTopic>(kWinBackTopic, "IRF_WinBackTopic");
		s.secretTopic = f.Own<RE::TESTopic>(kSecretTopic, "IRF_SecretTopic");
		s.dateTopic = f.Own<RE::TESTopic>(kDateTopic, "IRF_DateTopic");
		for (std::uint32_t i = 0; i < s.know.size(); ++i) {
			s.know[i] = f.Own<RE::TESTopic>(kKnowTopic1 + i, "IRF_KnowTopic");
		}
		s.denyTopic = f.Own<RE::TESTopic>(kDenyTopic, "IRF_DenyTopic");
		s.admitTopic = f.Own<RE::TESTopic>(kAdmitTopic, "IRF_AdmitTopic");
		s.giftInsulting = f.Own<RE::BGSKeyword>(kGiftInsultingKeyword, "IRF_GiftInsulting");
		s.give = f.Own<RE::TESFaction>(kGiveFaction, "IRF_GiveFaction");
		s.gift = f.Own<RE::TESFaction>(kGiftFaction, "IRF_GiftFaction");
		s.giveTopic = f.Own<RE::TESTopic>(kGiveTopic, "IRF_GiveTopic");
		s.giftAnswer = f.Own<RE::TESTopic>(kGiftAnswerTopic, "IRF_GiftAnswerTopic");
		s.child = f.Own<RE::TESFaction>(kChildFaction, "IRF_ChildFaction");
		s.childHello = f.Own<RE::TESTopic>(kChildHelloTopic, "IRF_ChildHelloTopic");
		s.adoptedBoys = f.Own<RE::TESGlobal>(kAdoptedBoys, "IRF_AdoptedBoys");
		s.adoptedGirls = f.Own<RE::TESGlobal>(kAdoptedGirls, "IRF_AdoptedGirls");
		s.moodFaction = f.Own<RE::TESFaction>(kMoodFaction, "IRF_MoodFaction");
		s.idle = f.Own<RE::TESTopic>(kIdleTopic, "IRF_IdleTopic");
		s.plead = f.Own<RE::TESFaction>(kPleadFaction, "IRF_PleadFaction");
		s.spouseBonus = f.Own<RE::TESFaction>(kSpouseBonusFaction, "IRF_SpouseBonusFaction");
		s.chat = f.Own<RE::TESFaction>(kChatFaction, "IRF_ChatFaction");
		s.firstDateQuest = f.Own<RE::TESQuest>(kFirstDateQuest, "IRF_FirstDateQuest");
		s.firstDateReply = f.Own<RE::TESTopic>(kFirstDateReplyTopic, "IRF_FirstDateReplyTopic");
		s.circleHello = f.Own<RE::TESTopic>(kCircleHelloTopic, "IRF_CircleHelloTopic");
		s.circleGoodbye = f.Own<RE::TESTopic>(kCircleGoodbyeTopic, "IRF_CircleGoodbyeTopic");
		s.pleadYes = f.Own<RE::TESTopic>(kPleadYesTopic, "IRF_PleadYesTopic");
		s.pleadNo = f.Own<RE::TESTopic>(kPleadNoTopic, "IRF_PleadNoTopic");
		s.forceGreetTopic = f.Own<RE::TESTopic>(kForceGreetTopic, "IRF_ForceGreetTopic");
		s.confessReply = f.Own<RE::TESTopic>(kConfessReplyTopic, "IRF_ConfessReplyTopic");
		s.sayQuest = f.Own<RE::TESQuest>(kSayQuest, "IRF_SayQuest");
		s.adoption = data->LookupForm<RE::TESQuest>(0x0042B4, "HearthFires.esm"sv);
		s.adoptionFaction = data->LookupForm<RE::TESFaction>(0x0042B0, "HearthFires.esm"sv);

		s.potentialMarriage = f.Vanilla<RE::TESFaction>(0x019809, "PotentialMarriageFaction");
		s.playerMarried = f.Vanilla<RE::TESFaction>(0x0C6472, "PlayerMarriedFaction");
		s.currentFollower = f.Vanilla<RE::TESFaction>(0x05C84E, "CurrentFollowerFaction");
		s.brawl = f.Vanilla<RE::TESFaction>(0x04CFA6, "DGIntimidateFaction");
		s.marriage = f.Vanilla<RE::TESQuest>(0x074793, "RelationshipMarriage");
		s.wedding = f.Vanilla<RE::TESQuest>(0x07404E, "RelationshipMarriageWedding");
		s.followerQuest = f.Vanilla<RE::TESQuest>(0x0750BA, "DialogueFollower");
		s.amuletOfMara = f.Vanilla<RE::TESObjectARMO>(0x0C891B, "ReligiousSymbolMaraLove");
		s.loversComfort = f.Vanilla<RE::SpellItem>(0x0CDA1D, "MarriageRested");
		s.speechVeryHard = f.Vanilla<RE::TESGlobal>(0x0D1954, "SpeechVeryHard");
		s.dragon = f.Vanilla<RE::BGSKeyword>(0x035D59, "ActorTypeDragon");
		s.locPlayerHouse = f.Vanilla<RE::BGSKeyword>(0x0FC1A3, "LocTypePlayerHouse");
		s.locHouse = f.Vanilla<RE::BGSKeyword>(0x01CB85, "LocTypeHouse");
		s.locDwelling = f.Vanilla<RE::BGSKeyword>(0x0130DC, "LocTypeDwelling");
		s.locInn = f.Vanilla<RE::BGSKeyword>(0x01CB87, "LocTypeInn");
		s.locCity = f.Vanilla<RE::BGSKeyword>(0x013168, "LocTypeCity");
		s.locTown = f.Vanilla<RE::BGSKeyword>(0x013166, "LocTypeTown");
		s.locSettlement = f.Vanilla<RE::BGSKeyword>(0x013167, "LocTypeSettlement");
		s.locHabitation = f.Vanilla<RE::BGSKeyword>(0x039793, "LocTypeHabitation");
		s.locDungeon = f.Vanilla<RE::BGSKeyword>(0x0130DB, "LocTypeDungeon");
		s.locNordicRuin = f.Vanilla<RE::BGSKeyword>(0x0130F2, "LocSetNordicRuin");
		s.locDwarvenRuin = f.Vanilla<RE::BGSKeyword>(0x0130F0, "LocSetDwarvenRuin");
		s.locCave = f.Vanilla<RE::BGSKeyword>(0x0130EF, "LocSetCave");
		return f.ok;
	}

	const Set& Get()
	{
		return g_set;
	}
}
