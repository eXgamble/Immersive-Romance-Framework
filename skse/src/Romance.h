#pragma once

#include "Candidates.h"
#include "Types.h"

// A candidate's relationship with the player. What lines read lives in faction ranks on the candidate (saved with
// the game); the rest in State. All changes run on an SKSE task (callers from Papyrus and events queue them).
namespace Romance
{
	using IRF::Event;
	using IRF::Stage;
	using IRF::Who;
	using IRF::Why;

	Stage GetStage(RE::Actor* a_actor);
	int   GetApproval(RE::Actor* a_actor);

	// The player's main partner (home, children, the spouse): the one at the highest stage (married included), not an
	// ex; nullptr when none
	RE::Actor* Partner();

	// Every partner: each candidate from Dating to Married. Two-timing is possible (a candidate who doesn't know, or
	// competes), so straying, jealousy and getting caught look at each of them
	std::vector<RE::Actor*> Partners();
	bool                    IsPartner(RE::Actor* a_actor);

	// a_who knows you're seeing a_partner (she has learned it, or was there when it came out)
	bool Knows(RE::Actor* a_who, RE::Actor* a_partner);

	void SetStage(RE::Actor* a_actor, Stage a_stage, std::string_view a_cause);

	// Raise or lower approval; a_why says what kind of hurt it was (for the breakup it may cause)
	void ModApproval(RE::Actor* a_actor, int a_amount, std::string_view a_cause, Why a_why = Why::kNone);

	// Hurt costs a candidate's tolerance (or ends it at once when it's "never")
	void Hurt(RE::Actor* a_actor, const Candidates::Tolerance& a_tolerance, Why a_why, std::string_view a_cause, int a_times = 1);

	// Ends the relationship: who and why are recorded for the ex lines and win-back
	void BreakUp(RE::Actor* a_actor, Who a_who, Why a_why, bool a_unforgivable = false);

	// She caught you with someone else (or word got back and your denial failed); her line after a_delay seconds.
	// a_with: the one she caught you with, when known: from now on she knows about her
	void Caught(RE::Actor* a_partner, std::string_view a_cause, float a_delay = 0.0f, RE::Actor* a_with = nullptr);

	// The player strayed with a_other (a kiss, a date): every other partner who sees it catches you, and if a_other is
	// a partner too and one of them was there, a_other catches you as well; who else saw it may tell them later
	void Strayed(RE::Actor* a_other, std::string_view a_deed);

	// A Friend who knows you're taken and won't compete for you: she turns down a date ("askdate.taken")
	bool WontBeCourted(RE::Actor* a_actor);

	// Something happened with a candidate: pending reactions for everyone who reacts, the recent event, refusals
	void Notify(RE::Actor* a_candidate, Event a_event);

	// The candidate and the player spent time together (for absence)
	void Contact(RE::Actor* a_actor);

	// Brings a candidate's ranks in line with the save. a_talking: the player is about to talk to them (absence,
	// word getting back and the answers are worked out then). Cheap; run on look, talk and load.
	void Sync(RE::Actor* a_actor, bool a_talking = false);
	void SyncAll();

	// IRF_ChatFaction: 1 while she follows you and may chat (not hurt or silent, no reaction, plea or confession
	// waiting). An add-on's own follower comments (towns, guilds, quests, bandit camps) need only this one condition, so
	// a comment never plays over a breakup. Part of Sync; also run when a moment starts or ends outside it
	void SyncChat(RE::Actor* a_actor);

	// A circle member or anyone with a pending reaction: their ranks (reaction, refusal)
	void SyncOther(RE::Actor* a_actor);

	// Starts any framework quest a save doesn't have running (a save made before an update)
	void StartQuests();

	bool IsPlayerInterested(const Candidates::Candidate& a_candidate);
}
