#pragma once

#include "Types.h"

// The first date (user, 2026-10-25): an eligible Friend is always open to "Would you spend an evening with me?". Below
// askDateApproval she turns you down, a Friend who knows you're taken (and won't compete) turns you down for it, and
// from askDateApproval she says yes ("askdate.yes": where and when; asked from 5 pm, "askdate.tomorrow") and this
// starts: tonight, or tomorrow night when asked from 5 pm (user, 2026-10-27). IRF_FirstDateQuest's objective says
// where; from sunset that evening her IRF_FirstDateWait package takes her to her date spot (her add-on's
// firstDate.spot) to wait (its condition reads IRF_DateFrom). While she's there that evening her greeting is the
// date: you go up to her and talk (no force greet, user 2026-10-27), she says "firstdate" (a hello, moment
// kFirstDate), you answer "Of course I came." ("firstdate.reply"), the talk ends in a kiss, she says
// "firstdate.after" and you're Dating. Not there by the end of the evening: you stood her up (approval, her "stoodup"
// line at your next meeting), ask again another day. Going on it while you have a partner is straying. One date at a
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

	// Her "firstdate" greeting began: you're on the date
	void OnSaid(RE::Actor* a_actor);

	// A talk ended: if it was the date, the kiss and Dating
	void OnDialogueClosed();

	void OnGameLoaded();
}
