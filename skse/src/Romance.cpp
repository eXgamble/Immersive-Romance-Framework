#include "Romance.h"

#include "Candidates.h"
#include "Forms.h"

namespace Romance
{
	namespace
	{
		bool InFaction(RE::Actor* a_actor, RE::TESFaction* a_faction)
		{
			return a_actor && a_faction && a_actor->IsInFaction(a_faction);
		}

		int Rank(RE::Actor* a_actor, RE::TESFaction* a_faction)
		{
			return InFaction(a_actor, a_faction) ? a_actor->GetFactionRank(a_faction, false) : -1;
		}

		void SetRank(RE::Actor* a_actor, RE::TESFaction* a_faction, int a_rank)
		{
			if (Rank(a_actor, a_faction) != a_rank) {
				a_actor->AddToFaction(a_faction, static_cast<std::int8_t>(a_rank));
			}
		}

		// Vanilla's relationship rank with the player (4 Lover .. -4 Archnemesis); 0 without a relationship record
		int RelationshipRank(RE::Actor* a_actor)
		{
			const auto npc = a_actor->GetActorBase();
			const auto player = RE::PlayerCharacter::GetSingleton()->GetActorBase();
			const auto rel = npc && player ? RE::BGSRelationship::GetRelationship(npc, player) : nullptr;
			return rel ? 4 - static_cast<int>(rel->level.underlying()) : 0;
		}

		GreetState GreetFor(Stage a_stage)
		{
			switch (a_stage) {
			case Stage::kCourting:
				return GreetState::kCourting;
			case Stage::kDating:
				return GreetState::kDating;
			case Stage::kCommitted:
				return GreetState::kCommitted;
			case Stage::kEngaged:
				return GreetState::kEngaged;
			case Stage::kMarried:
				return GreetState::kSpouse;
			default:
				return GreetState::kNone;
			}
		}

		void SendStageEvent(RE::Actor* a_actor, Stage a_stage, std::string_view a_cause)
		{
			SKSE::ModCallbackEvent event{ "IRF_StageChanged"sv, RE::BSFixedString(a_cause), static_cast<float>(a_stage), a_actor };
			SKSE::GetModCallbackEventSource()->SendEvent(&event);
		}
	}

	const std::vector<std::pair<GreetState, const char*>>& GreetStates()
	{
		static const std::vector<std::pair<GreetState, const char*>> states{
			{ GreetState::kCourting, "courting" },
			{ GreetState::kDating, "dating" },
			{ GreetState::kCommitted, "committed" },
			{ GreetState::kEngaged, "engaged" },
			{ GreetState::kSpouse, "spouse" },
		};
		return states;
	}

	Stage GetStage(RE::Actor* a_actor)
	{
		return static_cast<Stage>(Rank(a_actor, Forms::Get().stage));
	}

	int GetApproval(RE::Actor* a_actor)
	{
		return std::max(0, Rank(a_actor, Forms::Get().approval));
	}

	void SetStage(RE::Actor* a_actor, Stage a_stage, std::string_view a_cause)
	{
		if (!Candidates::Find(a_actor)) {
			return;
		}
		const auto old = GetStage(a_actor);
		if (old == a_stage) {
			return;
		}
		const auto& f = Forms::Get();
		if (a_stage == Stage::kNone) {
			a_actor->RemoveFromFaction(f.stage);
		} else {
			SetRank(a_actor, f.stage, static_cast<int>(a_stage));
		}
		logger::info("Romance: {} {} -> {} ({})", a_actor->GetDisplayFullName(), static_cast<int>(old), static_cast<int>(a_stage), a_cause);
		Sync(a_actor);
		SendStageEvent(a_actor, a_stage, a_cause);
	}

	void ModApproval(RE::Actor* a_actor, int a_amount, std::string_view a_cause)
	{
		if (!Candidates::Find(a_actor) || a_amount == 0) {
			return;
		}
		const auto before = GetApproval(a_actor);
		const auto after = std::clamp(before + a_amount, 0, 100);
		SetRank(a_actor, Forms::Get().approval, after);
		logger::info("Romance: {} approval {} -> {} ({:+}, {})", a_actor->GetDisplayFullName(), before, after, a_amount, a_cause);
	}

	void Sync(RE::Actor* a_actor)
	{
		if (!Candidates::Find(a_actor)) {
			return;
		}
		const auto& f = Forms::Get();
		SetRank(a_actor, f.candidate, 0);

		auto stage = GetStage(a_actor);
		if (stage == Stage::kNone && RelationshipRank(a_actor) >= 1) {
			SetRank(a_actor, f.stage, static_cast<int>(Stage::kFriend));
			logger::info("Romance: {} is now a Friend", a_actor->GetDisplayFullName());
			stage = Stage::kFriend;
		}
		if (stage != Stage::kNone && !InFaction(a_actor, f.approval)) {
			SetRank(a_actor, f.approval, kStartApproval);
		}
		SetRank(a_actor, f.greet, static_cast<int>(GreetFor(stage)));

		// The amulet gate: vanilla's proposal needs PotentialMarriageFaction, so a candidate is out of it until
		// Committed. Vanilla scripts add it back now and then (Lydia's housecarl line, the 1.5 update quest).
		const bool mayMarry = stage >= Stage::kCommitted;
		if (mayMarry && !InFaction(a_actor, f.potentialMarriage)) {
			a_actor->AddToFaction(f.potentialMarriage, 0);
		} else if (!mayMarry && InFaction(a_actor, f.potentialMarriage) && Rank(a_actor, f.potentialMarriage) >= 0) {
			a_actor->RemoveFromFaction(f.potentialMarriage);
		}
	}

	void SyncAll()
	{
		for (const auto& candidate : Candidates::All()) {
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(candidate.ref)) {
				Sync(actor);
			}
		}
	}
}
