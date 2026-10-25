#pragma once

#include "Types.h"

// Looking at the world around the player: who sees what, how private a place is, and making someone speak.
// Every check runs once at a moment (a kiss, a crime, a gift), never per frame.
namespace World
{
	// a_watcher can see a_target right now (within a_distance, the game's own detection)
	bool Sees(RE::Actor* a_watcher, RE::Actor* a_target, float a_distance);

	// Loaded actors (high process) within a_distance who see a_target, the player and their followers left out
	std::vector<RE::Actor*> Witnesses(RE::Actor* a_target, float a_distance);

	// Would a candidate with this modesty kiss here: 0 anywhere, 1 nobody watching, 2 indoors and nobody watching,
	// 3 only in a home (a house, a dwelling or the player's house) with nobody watching
	bool PrivateEnough(RE::Actor* a_partner, int a_modesty);

	bool Roll(int a_percent);

	// The speaker says their line for a moment (IRF_SayTopic, gated on IRF_MomentFaction), after a_delay seconds of
	// game time when given (a line said while a dialogue is still closing never plays)
	void SayMoment(RE::Actor* a_speaker, IRF::Moment a_moment, float a_delay = 0.0f);

	// The road line for a place, for a partner whose community is a_home; kNone when the place has none
	IRF::Moment MomentForPlace(RE::BGSLocation* a_location, RE::BGSLocation* a_home);

	// A kiss: the screen fades for a moment (the animation provider comes later)
	void Kiss(RE::Actor* a_partner);
}
