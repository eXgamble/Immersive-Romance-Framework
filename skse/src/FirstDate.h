#pragma once

#include "Types.h"

// The first date (user, 2026-10-25; the evening itself 2026-10-27): an eligible Friend is always open to "Would you
// spend an evening with me?". Below askDateApproval she turns you down, a Friend who knows you're taken (and won't
// compete) turns you down for it, and from askDateApproval she says yes ("askdate.yes": where and when; asked from
// 5 pm, "askdate.tomorrow", tomorrow night) and this starts. IRF_FirstDateQuest's objective says where; from sunset
// that evening her IRF_FirstDateWait package takes her to her date spot (her add-on's firstDate.spot) to wait standing
// (its condition reads IRF_DateFrom). Not there by the end of the evening: you stood her up (approval, her "stoodup"
// line at your next meeting), ask again another day.
//
// The evening, in steps (IRF_DatePhase):
// - the talk: at her spot after sunset her greeting is the date ("firstdate", a hello: you go up to her and talk).
//   Her add-on's options follow ("date.talk.N": your words, her answer; then your two replies ".a" / ".b" and hers to
//   them, back to the options). A used option is gone; after the first, "(Stay until the fire burns low.)"
//   ("date.end") ends the talk, as leaving it does;
// - the fade: eveningHours pass, nothing moves; she comes up to you: "Walk me home?" ("date.walkhome", your two
//   answers "date.walkhome.1" / ".2");
// - the walk: she follows you (IRF_FirstDateFollow), the journal says "Walk <her> home" with a marker on her door
//   (her add-on's evening.home, from outside). Sleep, wait and fast travel are refused, and straying more than
//   dateStrayDistance from her turns you back toward her (user: as Fated Start does at the camp);
// - her door (dateHomeDistance): she comes up to you ("date.door"): "(Kiss her.)" ("date.kiss") is the kiss,
//   "firstdate.after" and Dating; "Goodnight." ("date.goodnight", or leaving the talk) leaves her a Friend, and
//   "Will you be mine?" ("mine.yes", while approval is at least mineApproval) makes her Dating later.
// An add-on without the talk gets the closing option at once; without the walk (or evening.home) the door scene comes
// right after the fade; without the door, the kiss. Going on it while you have a partner is straying. One date at a
// time.
namespace FirstDate
{
	// "askdate.yes" / "askdate.tomorrow" was said: tonight (tomorrow night from 5 pm), at her spot
	void Start(RE::Actor* a_actor);

	// A date is set with someone (the ask-out line waits until it's over)
	bool Pending();

	// Is she at her spot and is it time (her date greeting on), or did you miss it? Called when something changes that
	// could make it so: a location change, a loading screen closing, a fight ending, waking or waiting, looking at her
	// (so it's always on before you talk to her), a talk ending
	void Check();

	// Her "firstdate" greeting began: the talk is on
	void OnSaid(RE::Actor* a_actor);

	// One of her date lines began ("date.talk.N", "date.walkhome", "date.door", "date.kiss")
	void OnLineBegan(RE::Actor* a_actor, const std::string& a_key);

	// A talk ended: the date's next step
	void OnDialogueClosed();

	// Papyrus (IRF_Internal.DateFade): the fade is over
	void Faded(RE::Actor* a_actor);

	// Papyrus (IRF_Internal.WalkHome, VM thread): queue the walk's check; false once the walk is over
	bool WalkTick(std::int32_t a_walk);

	// Papyrus (IRF_Internal.TurnBack): you're back with her
	void TurnedBack();

	// From the talk's end to her door: no sleeping, waiting or fast travel, and why
	bool        Blocking();
	std::string BlockMessage();

	// Before a load: forget this session's date
	void OnGameLoaded();
	// After it (or a new game): a date saved halfway carries on from its step
	void Resume();
}
