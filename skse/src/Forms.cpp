#include "Forms.h"

namespace Forms
{
	namespace
	{
		Set g_set;

		template <class T>
		T* Find(RE::TESDataHandler* a_data, RE::FormID a_local, std::string_view a_plugin, const char* a_what, bool& a_ok)
		{
			auto form = a_data->LookupForm<T>(a_local, a_plugin);
			if (!form) {
				logger::error("Forms: {} ({:06X} in {}) not found", a_what, a_local, a_plugin);
				a_ok = false;
			}
			return form;
		}
	}

	bool Load()
	{
		auto data = RE::TESDataHandler::GetSingleton();
		if (!data || !data->LookupModByName(kPlugin)) {
			logger::error("Forms: {} is not loaded; the framework stays off", kPlugin);
			return false;
		}
		bool ok = true;
		g_set.candidate = Find<RE::TESFaction>(data, kCandidateFaction, kPlugin, "IRF_CandidateFaction", ok);
		g_set.stage = Find<RE::TESFaction>(data, kStageFaction, kPlugin, "IRF_StageFaction", ok);
		g_set.approval = Find<RE::TESFaction>(data, kApprovalFaction, kPlugin, "IRF_ApprovalFaction", ok);
		g_set.greet = Find<RE::TESFaction>(data, kGreetFaction, kPlugin, "IRF_GreetFaction", ok);
		g_set.questBusy = Find<RE::TESFaction>(data, kQuestBusyFaction, kPlugin, "IRF_QuestBusyFaction", ok);
		g_set.quest = Find<RE::TESQuest>(data, kQuest, kPlugin, "IRF_Quest", ok);
		g_set.hello = Find<RE::TESTopic>(data, kHelloTopic, kPlugin, "IRF_HelloTopic", ok);
		g_set.goodbye = Find<RE::TESTopic>(data, kGoodbyeTopic, kPlugin, "IRF_GoodbyeTopic", ok);
		g_set.court = Find<RE::TESTopic>(data, kCourtTopic, kPlugin, "IRF_CourtTopic", ok);
		g_set.howAreWe = Find<RE::TESTopic>(data, kHowAreWeTopic, kPlugin, "IRF_HowAreWeTopic", ok);
		g_set.amulet = Find<RE::TESFaction>(data, kAmuletFaction, kPlugin, "IRF_AmuletFaction", ok);
		g_set.amuletTopic = Find<RE::TESTopic>(data, kAmuletTopic, kPlugin, "IRF_AmuletTopic", ok);
		g_set.potentialMarriage = Find<RE::TESFaction>(data, 0x019809, "Skyrim.esm"sv, "PotentialMarriageFaction", ok);
		g_set.playerMarried = Find<RE::TESFaction>(data, 0x0C6472, "Skyrim.esm"sv, "PlayerMarriedFaction", ok);
		g_set.marriage = Find<RE::TESQuest>(data, 0x074793, "Skyrim.esm"sv, "RelationshipMarriage", ok);
		g_set.wedding = Find<RE::TESQuest>(data, 0x07404E, "Skyrim.esm"sv, "RelationshipMarriageWedding", ok);
		g_set.amuletOfMara = Find<RE::TESObjectARMO>(data, 0x0C891B, "Skyrim.esm"sv, "ReligiousSymbolMaraLove", ok);
		return ok;
	}

	const Set& Get()
	{
		return g_set;
	}
}
