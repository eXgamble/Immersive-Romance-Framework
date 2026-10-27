#pragma once

#include "Types.h"

// The scene director (user, 2026-10-26): one staged scene at a time. Every moment the framework stages with a force
// greet, a walk or a hold (she won't leave town, two partners have it out, the first date, her confession, a family
// conversation) takes the stage here first, so two scenes never fight over the same alias, package or moment line.
// A scene that can't have the stage waits: its own check tries again at the next chance (Expose falls back to both
// simply catching you). While a scene is on, its actors say only its lines (and a fight's), and a scene that asks for
// it gets a beat every couple of seconds (its clock when no line or look comes along: a line that never played).
namespace Scenes
{
	// in priority order: a scene may break off one below it while that one still allows it, never one at or above it
	enum class Kind : std::uint8_t
	{
		kNone,
		kFamily,     // a family conversation
		kConfess,    // newly Committed, she comes to tell you
		kEvening,    // an evening together: the fade, the walk out, her line after
		kFirstDate,  // she comes over at her spot, and the talk
		kExpose,     // two partners have it out
		kStayHome,   // a wife with children won't leave town (you're leaving: it can't wait)
	};

	// Takes the stage for these actors. a_breakOff ends the scene when a more important one needs the stage (none:
	// it can't be broken off); a_beat runs every couple of seconds while it's on. False when another scene holds the
	// stage (one on for over two game hours is stuck and gives it up).
	bool Begin(Kind a_kind, std::initializer_list<RE::Actor*> a_cast, std::function<void()> a_breakOff, std::function<void()> a_beat = {});
	void Settle(Kind a_kind);  // past the point of breaking off (the talk is on): it keeps the stage until it ends
	void End(Kind a_kind);     // nothing unless it's the scene on
	bool On(Kind a_kind);

	// May this line play? A scene's actors say only its own lines (and a fight's)
	bool MaySay(RE::Actor* a_speaker, IRF::Moment a_moment);
	void OnBeat(int a_beat);  // Papyrus: IRF_Internal.BeatLater waited
	void OnGameLoaded();
}
