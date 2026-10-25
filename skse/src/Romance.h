#pragma once

// A candidate's relationship with the player. Everything lives in faction ranks on the candidate, so it is saved
// with the game and dialogue conditions read it directly:
//   IRF_StageFaction     the stage (not a member before Friend)
//   IRF_ApprovalFaction  approval 0..100
//   IRF_GreetFaction     the greeting state the add-on's hellos and goodbyes are gated on
namespace Romance
{
	enum class Stage : std::int8_t
	{
		kNone = -1,  // not a Friend yet (vanilla relationship rank below 1)
		kFriend = 0,
		kCourting = 1,
		kDating = 2,
		kCommitted = 3,
		kEngaged = 4,
		kMarried = 5,
	};

	// Greeting states, the rank IRF_GreetFaction holds; 0 = none (vanilla greets them)
	enum class GreetState : std::int8_t
	{
		kNone = 0,
		kCourting = 1,
		kDating = 2,
		kCommitted = 3,
		kEngaged = 4,
		kSpouse = 5,
	};

	// The states that need lines, with the name the manifest uses ("hello.<name>", "goodbye.<name>")
	const std::vector<std::pair<GreetState, const char*>>& GreetStates();

	inline constexpr int kStartApproval = 20;

	Stage GetStage(RE::Actor* a_actor);
	int   GetApproval(RE::Actor* a_actor);

	// Game-state changes: call from an SKSE task (they're queued for you from Papyrus and events)
	void SetStage(RE::Actor* a_actor, Stage a_stage, std::string_view a_cause);
	void ModApproval(RE::Actor* a_actor, int a_amount, std::string_view a_cause);

	// Brings a candidate's ranks in line with the save: Friend once vanilla's rank reaches 1, the greeting state,
	// and the amulet gate (out of vanilla's marriage faction until Committed). Cheap; run on look, talk and load.
	void Sync(RE::Actor* a_actor);
	void SyncAll();
}
