#pragma once

// The partner on the road: recruited and dismissed through vanilla's follower system (so any follower count and
// vanilla's Wait / Trade / Dismiss lines work), with the add-on's combat profile on while they follow, and growth
// earned the way the player earns levels (fighting, clearing places).
namespace Follower
{
	void Recruit(RE::Actor* a_actor);
	void Dismiss(RE::Actor* a_actor);

	// Applies or removes the combat profile to match whether they follow; re-applies the base-record parts after a load
	void SyncProfile(RE::Actor* a_actor);

	// Growth: experience for a fight or a cleared place
	void AddXp(RE::Actor* a_actor, int a_xp, std::string_view a_why);

	void OnGameLoaded();
}
