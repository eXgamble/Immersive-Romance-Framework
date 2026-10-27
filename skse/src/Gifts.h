#pragma once

#include "Candidates.h"

// Gifts (moved from eXrim, 2026-10-25): the player's "I have something for you." (IRF_GiveTopic, offered to a
// Friend once a day, or a wife once every wifeGiftEveryDays, who isn't giving you the silent treatment; Dating to
// Engaged she declines with her "gift.decline" line,
// IRF_GiveFaction 2, and would rather you took her along) opens the game's own gift menu over the
// conversation, the player's things with stolen and quest items refused and a gold row added. A pick is handed over,
// the menus close and her goodbye is her answer (IRF_GiftAnswerTopic, by IRF_GiftFaction). How she takes it:
// insulted, disliked, neutral, liked or loved, from her manifest's lists, then IRF_GiftInsulting (items the average
// person takes as an insult; the framework tags the ones listed in Gifts/*.json at load, other mods may add the
// keyword to their own), and gold by the amount (Rules.json giftGoldLiked, giftGoldLoved). An insult isn't taken.
namespace Gifts
{
	// kDataLoaded: tag the insulting items, the dialogue and menu event sinks
	void Load();

	// How this candidate takes a_count of this item
	IRF::Taste TasteOf(const Candidates::Candidate& a_candidate, RE::TESForm* a_item, std::int32_t a_count);

	void OnGameLoaded();
}
