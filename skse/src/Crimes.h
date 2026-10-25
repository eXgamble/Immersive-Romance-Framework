#pragma once

// Crimes a candidate sees, judged by their alignment (-5 evil .. 5 good). The game has no crime event, so the
// player's crime-gold virtual functions are hooked (they run for every crime with a bounty, measured 2026-10-24);
// one task then reads the game's live crimes and who knows of them. A following partner is never on that list, so
// they get their own sight check. A theft too cheap for a bounty is not seen this way (known gap).
namespace Crimes
{
	void Install();
	void OnGameLoaded();
}
