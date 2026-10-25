#include "Follower.h"

#include "Candidates.h"
#include "Forms.h"
#include "Rules.h"
#include "Script.h"
#include "State.h"

namespace Follower
{
	namespace
	{
		// base records whose combat style we swapped this session (a base-record change isn't saved)
		std::unordered_map<RE::TESNPC*, RE::TESCombatStyle*> g_originalStyles;

		RE::ActorValue SkillFor(const std::string& a_role)
		{
			if (a_role == "archer") {
				return RE::ActorValue::kArchery;
			}
			if (a_role == "mage") {
				return RE::ActorValue::kDestruction;
			}
			if (a_role == "support") {
				return RE::ActorValue::kRestoration;
			}
			if (a_role == "heavy") {
				return RE::ActorValue::kTwoHanded;
			}
			return RE::ActorValue::kOneHanded;
		}

		// The growth levels a candidate can still gain: their cap minus their own level
		int GrowthRoom(RE::Actor* a_actor, const Candidates::Candidate& a_candidate)
		{
			return std::max(0, a_candidate.attributes.combat.levelCap - static_cast<int>(a_actor->GetLevel()));
		}

		void ApplyGrowth(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, State::Candidate& a_state)
		{
			const auto& r = Rules::Get();
			const int   wanted = std::min(GrowthRoom(a_actor, a_candidate), a_state.partnerXp / std::max(1, r.partnerXpPerLevel));
			const int   delta = wanted - a_state.appliedLevels;
			if (delta <= 0) {
				return;
			}
			const auto av = a_actor->AsActorValueOwner();
			av->SetBaseActorValue(RE::ActorValue::kHealth, av->GetBaseActorValue(RE::ActorValue::kHealth) + static_cast<float>(delta * r.partnerHealthPerLevel));
			const auto skill = SkillFor(a_candidate.attributes.combat.role);
			av->SetBaseActorValue(skill, av->GetBaseActorValue(skill) + static_cast<float>(delta * r.partnerSkillPerLevel));
			a_state.appliedLevels = wanted;
			logger::info("Follower: {} grew {} level(s) ({} of {} so far)", a_actor->GetDisplayFullName(), delta, wanted, GrowthRoom(a_actor, a_candidate));
		}

		void SwapStyle(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, bool a_on)
		{
			const auto npc = a_actor->GetActorBase();
			const auto style = a_candidate.attributes.combat.style;
			if (!npc || !style) {
				return;
			}
			if (a_on && !g_originalStyles.contains(npc)) {
				g_originalStyles[npc] = npc->combatStyle;
				npc->combatStyle = style;
			} else if (!a_on) {
				if (const auto it = g_originalStyles.find(npc); it != g_originalStyles.end()) {
					npc->combatStyle = it->second;
					g_originalStyles.erase(it);
				}
			}
		}

		void Apply(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, State::Candidate& a_state)
		{
			const auto& combat = a_candidate.attributes.combat;
			const auto  av = a_actor->AsActorValueOwner();
			if (combat.confidence >= 0.0f) {
				a_state.savedConfidence = av->GetBaseActorValue(RE::ActorValue::kConfidence);
				av->SetBaseActorValue(RE::ActorValue::kConfidence, combat.confidence);
			}
			if (combat.aggression >= 0.0f) {
				a_state.savedAggression = av->GetBaseActorValue(RE::ActorValue::kAggression);
				av->SetBaseActorValue(RE::ActorValue::kAggression, combat.aggression);
			}
			for (const auto item : combat.gear) {
				a_actor->AddObjectToContainer(item, nullptr, 1, nullptr);
				if (item->IsArmor() || item->IsWeapon()) {
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, item);
				}
			}
			SwapStyle(a_actor, a_candidate, true);
			a_state.profileApplied = true;
			logger::info("Follower: {}'s combat profile on ({})", a_actor->GetDisplayFullName(), combat.role.empty() ? "no role" : combat.role);
		}

		void Remove(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, State::Candidate& a_state)
		{
			const auto& combat = a_candidate.attributes.combat;
			const auto  av = a_actor->AsActorValueOwner();
			if (a_state.savedConfidence >= 0.0f) {
				av->SetBaseActorValue(RE::ActorValue::kConfidence, a_state.savedConfidence);
				a_state.savedConfidence = -1.0f;
			}
			if (a_state.savedAggression >= 0.0f) {
				av->SetBaseActorValue(RE::ActorValue::kAggression, a_state.savedAggression);
				a_state.savedAggression = -1.0f;
			}
			for (const auto item : combat.gear) {
				a_actor->RemoveItem(item, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			}
			SwapStyle(a_actor, a_candidate, false);
			a_state.profileApplied = false;
			logger::info("Follower: {}'s combat profile off", a_actor->GetDisplayFullName());
		}
	}

	void Recruit(RE::Actor* a_actor)
	{
		const auto& f = Forms::Get();
		Script::Call(f.followerQuest, "DialogueFollowerScript"sv, "SetFollower"sv, static_cast<RE::TESObjectREFR*>(a_actor));
		State::Of(a_actor->GetFormID()).lastContactDay = State::Today();
		logger::info("Follower: {} comes with you", a_actor->GetDisplayFullName());
	}

	void Dismiss(RE::Actor* a_actor)
	{
		const auto& f = Forms::Get();
		if (!a_actor || !a_actor->IsPlayerTeammate() || !a_actor->IsInFaction(f.currentFollower)) {
			return;
		}
		// DismissFollower(iMessage = 0, iSayLine = 1): no message box, no vanilla farewell line
		Script::Call(f.followerQuest, "DialogueFollowerScript"sv, "DismissFollower"sv, static_cast<std::int32_t>(0), static_cast<std::int32_t>(0));
		logger::info("Follower: {} stops following", a_actor->GetDisplayFullName());
	}

	void SyncProfile(RE::Actor* a_actor)
	{
		const auto candidate = Candidates::Find(a_actor);
		if (!candidate) {
			return;
		}
		auto&      s = State::Of(a_actor->GetFormID());
		const bool following = a_actor->IsPlayerTeammate();
		if (following && !s.profileApplied) {
			Apply(a_actor, *candidate, s);
		} else if (!following && s.profileApplied) {
			Remove(a_actor, *candidate, s);
		} else if (following) {
			SwapStyle(a_actor, *candidate, true);  // after a load the base record is vanilla again
		}
		if (following) {
			ApplyGrowth(a_actor, *candidate, s);
		}
	}

	void AddXp(RE::Actor* a_actor, int a_xp, std::string_view a_why)
	{
		const auto candidate = Candidates::Find(a_actor);
		if (!candidate || candidate->attributes.combat.levelCap <= 0 || a_xp <= 0) {
			return;
		}
		auto& s = State::Of(a_actor->GetFormID());
		s.partnerXp += a_xp;
		logger::info("Follower: {} +{} xp ({}), {} total", a_actor->GetDisplayFullName(), a_xp, a_why, s.partnerXp);
		ApplyGrowth(a_actor, *candidate, s);
	}

	void OnGameLoaded()
	{
		// base records stay in memory across loads in one session: put every swapped style back; SyncProfile swaps
		// it again for whoever follows in the loaded save
		for (const auto& [npc, style] : g_originalStyles) {
			npc->combatStyle = style;
		}
		g_originalStyles.clear();
	}
}
