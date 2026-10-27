#pragma once

#include "Types.h"

// Looking at the world around the player: who sees what, how private a place is, and making someone speak.
// Every check runs once at a moment (a kiss, a crime, a gift), never per frame.
namespace World
{
	// a_watcher can see a_target right now (within a_distance, the game's own detection)
	bool Sees(RE::Actor* a_watcher, RE::Actor* a_target, float a_distance);

	// a_watcher sees a_target with her own eyes: within a_distance, in front of her (half circle), and nothing solid
	// between her eyes and their head (a physics ray: GetLineOfSight never works between two NPCs)
	bool InSight(RE::Actor* a_watcher, RE::Actor* a_target, float a_distance);

	// Loaded actors (high process) within a_distance who see a_target, the player and their followers left out
	std::vector<RE::Actor*> Witnesses(RE::Actor* a_target, float a_distance);

	// Would a candidate with this modesty kiss here: 0 anywhere, 1 nobody watching, 2 indoors and nobody watching,
	// 3 only in a home (a house, a dwelling or the player's house) with nobody watching
	bool PrivateEnough(RE::Actor* a_partner, int a_modesty);

	bool Roll(int a_percent);

	// Both in the same interior, or both outside in the same worldspace
	bool SamePlace(RE::TESObjectREFR* a_a, RE::TESObjectREFR* a_b);

	// 18:00 to 23:30: evening (the first date waits for it)
	bool Evening();

	// "Spend the evening with me.": dateFromHour to dateToHour, in a town (a city, town, settlement, inn or home), not
	// in combat
	bool DateTime();

	// The speaker says their line for a moment (IRF_SayTopic, gated on IRF_MomentFaction), after a_delay seconds of
	// game time when given (a line said while a dialogue is still closing never plays)
	void SayMoment(RE::Actor* a_speaker, IRF::Moment a_moment, float a_delay = 0.0f);

	// The road line for a place, for a partner whose community is a_home; kNone when the place has none
	IRF::Moment MomentForPlace(RE::BGSLocation* a_location, RE::BGSLocation* a_home);

	// A kiss: the screen fades for a moment (the animation provider comes later)
	void Kiss(RE::Actor* a_partner);
}
