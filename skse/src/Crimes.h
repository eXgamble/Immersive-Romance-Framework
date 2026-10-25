#pragma once

// Crimes a candidate sees, judged by their alignment (-5 evil .. 5 good). The game has no crime event, so the
// player's crime-gold virtual functions are hooked (they run for every crime with a bounty, measured 2026-10-24);
// one task then reads the game's live crimes and who knows of them. A following partner is never on that list, so
// they get their own sight check. Thefts are judged from the item instead (a cheap theft raises no bounty): an item
// that lands in your pack still owned by someone else was stolen, and a candidate who sees you take it judges it.
namespace Crimes
{
	void Install();
	void OnGameLoaded();

	// An item entered the player's inventory (TESContainerChangedEvent); call from an SKSE task
	void OnItemTaken(RE::FormID a_from, RE::FormID a_item, std::int32_t a_count);
	// An item left the player's inventory
	void OnItemGiven(RE::FormID a_item);
}
