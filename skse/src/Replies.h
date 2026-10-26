#pragma once

#include "Types.h"

// Replies (user, 2026-10-26): a line can have a short answer from the candidate, said aloud right after it. Sven's
// goodbye "You should have chosen me, Camilla." and Camilla answering him. The add-on's manifest lists the lines that
// get a reply ("replies": after these lines, these answers); her answers sit in IRF_SayTopic (moment kReply) with
// IRF_ReplyFaction == the reply's number, so the game picks among that reply's versions (her stage...) by their
// conditions. The line asked of her is only spoken when she's there to hear it (the add-on gives it a "near"
// condition), and once a day comes from the line itself (Say Once, reset after 24 hours).
namespace Replies
{
	// A line with a reply ended: she answers (when the talk it ended is over, if one is still open)
	void OnLineEnded(RE::Actor* a_speaker, RE::FormID a_candidate, int a_reply);

	// The dialogue menu closed: a reply that waited for it is said now
	void OnDialogueClosed();

	void OnGameLoaded();
}
