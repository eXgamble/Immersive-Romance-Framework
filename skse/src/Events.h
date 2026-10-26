#pragma once

// What the framework listens to. All event-driven; nothing runs between events.
//   crosshair, dialogue menu    the candidate you look at or talk to is synced (the right hello, the answers)
//   a line is said              what the add-on line means (court, ask, follow, kiss, amends, breakup, ...)
//   barter / training menu      a circle member who refuses you closes it and says why
//   bleedouts, hits, combat, deaths, cleared places, places, sleep   approval, growth and road lines (gifts: Gifts.cpp)
namespace Events
{
	void Register();

	// An attack on a candidate (a hit outside a fight, too many stray hits; debug): approval, lock, wary mood
	void Attack(RE::Actor* a_actor, std::string_view a_how);
	void OnGameLoaded();

	// Debug: runs the jealousy check now, past the once-a-day limit: true when she got jealous (a_what: of whom, a
	// fighter or not), else false and why not
	bool TestJealous(RE::Actor* a_partner, std::string& a_what);
}
