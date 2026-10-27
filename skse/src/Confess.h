#pragma once

#include "Types.h"

// Becoming Committed: a while after her approval reaches the commitment bar (confessDelayDays, still Dating; and at
// least confessMinDatingDays after you started Dating) she comes
// up to you, somewhere safe and out of combat, and tells you how much you mean to her (her add-on's "confess" line, no
// marriage hint). You get one reply ("You mean a lot to me too.") and she answers it ("confess.reply"); when that talk
// ends she is Committed. Approval falling below the bar first cancels it. Once per relationship (again only after a
// breakup and a win-back): later, or with the confession switched off, the bar makes her Committed at once.
// It uses the framework's one force greet (IRF_SayQuest's ForceGreet alias) with IRF_MomentFaction = kConfess.
namespace Confess
{
	// Does reaching the commitment bar wait for her to tell you? (the switch on, her line there, not said yet)
	bool Wanted(RE::Actor* a_partner);

	// Might she come over now? Called when something changes that could make it so: a location change, a loading
	// screen closing, a fight ending, looking at her
	void Check();

	// Her "confess" line began: it's said
	void OnSaid(RE::Actor* a_partner);

	// A talk ended: the force greet and its moment are over
	void OnDialogueClosed();

	void OnGameLoaded();
}
