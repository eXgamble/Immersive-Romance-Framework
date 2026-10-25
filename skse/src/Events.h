#pragma once

// What the framework listens to. All event-driven; nothing runs between events.
//   crosshair, dialogue menu    the candidate you look at or talk to is synced (the right hello, the answers)
//   a line is said              what the add-on line means (court, ask, follow, kiss, amends, breakup, ...)
//   barter / training menu      a circle member who refuses you closes it and says why
//   bleedouts, hits, combat, deaths, cleared places, places, sleep   approval, growth and road lines (gifts: Gifts.cpp)
namespace Events
{
	void Register();
	void OnGameLoaded();
}
