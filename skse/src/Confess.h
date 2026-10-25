#pragma once

#include "Types.h"

// Newly Committed: a while after she becomes Committed (confessDelayDays) she comes up to you, somewhere safe and out
// of combat, and tells you how much you mean to her (her add-on's "confess" line, no marriage hint). You get one reply
// ("You mean a lot to me too.") and she answers it ("confess.reply"). It's how the player learns she's at the next
// stage. Once per relationship: again only after a breakup and a win-back. Dropping back to Dating first cancels it.
// It uses the framework's one force greet (IRF_SayQuest's ForceGreet alias) with IRF_MomentFaction = kConfess.
namespace Confess
{
	// Might she come over now? Called when something changes that could make it so: a location change, a loading
	// screen closing, a fight ending, looking at her
	void Check();

	// Her "confess" line began: it's said
	void OnSaid(RE::Actor* a_partner);

	// A talk ended: the force greet and its moment are over
	void OnDialogueClosed();

	void OnGameLoaded();
}
