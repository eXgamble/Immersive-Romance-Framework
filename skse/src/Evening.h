#pragma once

#include "Types.h"

// An evening together (user, 2026-10-27): "Spend the evening with me." (6 to 10 pm, in a town, out of combat), the
// talk closes, the screen fades to black, eveningHours pass, and you come back with her outside the town's inn: the
// spot the inn's exit door puts you at (Evenings/*.json names each inn's exit door). In her own town while she isn't
// following you it's outside her home instead (her add-on's "evening": {"home": her home's exit door}). She says her
// "date.after" line, then goes back to her day (or keeps following you). A town without a spot offers no evening.
namespace Evening
{
	// kDataLoaded: the inns' exit doors (Evenings/*.json)
	void Load();

	// The door whose outside spot an evening with her ends at here (nullptr: none, no evening offered)
	RE::TESObjectREFR* Spot(RE::Actor* a_her);

	// Her "date" line was said in a talk: the evening starts once the talk closes
	void Begin(RE::Actor* a_her);
	void OnDialogueClosed();

	// Papyrus (IRF_Internal.EveningTogether): the screen is black, time passes; then you're both back outside
	void PassTime(float a_hours);
	void Done(RE::Actor* a_her);

	void OnGameLoaded();
}
