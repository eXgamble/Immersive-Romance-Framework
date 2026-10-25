#pragma once

#include "Candidates.h"

// Gifts: how a candidate takes one (insulted, disliked, neutral, liked, loved), what it does to approval, and her
// answer once the menus close. Items the average person takes as an insult carry IRF_GiftInsulting: the framework
// tags the ones listed in Data/SKSE/Plugins/ImmersiveRomanceFramework/Gifts/*.json at load, other mods may add the
// keyword to their own items.
namespace Gifts
{
	void Load();

	// How this candidate takes this item: her own lists first (an item named outright, then a keyword or form list),
	// then IRF_GiftInsulting
	IRF::Taste TasteOf(const Candidates::Candidate& a_candidate, RE::TESForm* a_item);

	// The player gave a_item to a candidate who is at least a Friend
	void OnGiven(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, RE::TESForm* a_item);

	// A menu closed: her answer to the strongest gift of the visit, once no dialogue is open any more
	void OnMenuClosed(std::string_view a_menu);

	void OnGameLoaded();
}
