#include "Confess.h"

#include "Candidates.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "Scenes.h"
#include "Script.h"
#include "State.h"
#include "World.h"

namespace Confess
{
	using namespace IRF;

	namespace
	{
		// game thread (SKSE tasks)
		RE::ActorHandle g_coming;              // on her way to tell you
		float           g_comingDay{ 0.0f };
		RE::ActorHandle g_said;                // said it; the force greet ends with the talk

		void Off(RE::Actor* a_partner)
		{
			Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_partner, false);
			if (a_partner->GetFactionRank(Forms::Get().moment, false) == Rank(Moment::kConfess)) {
				a_partner->AddToFaction(Forms::Get().moment, 0);
			}
			Romance::SyncChat(a_partner);
			Scenes::End(Scenes::Kind::kConfess);
		}

		// Following you she says it anywhere but a dungeon (a camp on the road, an inn); otherwise somewhere people live
		bool SafePlace(RE::Actor* a_partner, const Candidates::Candidate& a_candidate)
		{
			const auto& f = Forms::Get();
			const auto  loc = RE::PlayerCharacter::GetSingleton()->GetCurrentLocation();
			if (a_partner->IsPlayerTeammate()) {
				return !loc || !(loc->HasKeyword(f.locDungeon) || loc->HasKeyword(f.locCave) || loc->HasKeyword(f.locNordicRuin) ||
				                   loc->HasKeyword(f.locDwarvenRuin));
			}
			if (!loc) {
				return false;  // the wilds
			}
			if (a_candidate.community && Candidates::TownOf(loc) == a_candidate.community) {
				return true;
			}
			for (const auto keyword : { f.locCity, f.locTown, f.locSettlement, f.locInn, f.locPlayerHouse, f.locHouse, f.locDwelling }) {
				if (keyword && loc->HasKeyword(keyword)) {
					return true;
				}
			}
			return false;
		}

		bool Busy(RE::Actor* a_actor)
		{
			const auto& f = Forms::Get();
			return a_actor->IsDead() || a_actor->IsInCombat() || a_actor->IsInFaction(f.questBusy) && a_actor->GetFactionRank(f.questBusy, false) > 0 ||
			       a_actor->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kIsSleeping;
		}
	}

	void Check()
	{
		RE::Actor* partner = nullptr;
		for (const auto p : Romance::Partners()) {
			if (Romance::GetStage(p) == Stage::kCommitted && !State::Of(p->GetFormID()).confessed) {
				partner = p;  // with two partners, the first who hasn't told you yet
				break;
			}
		}
		const auto today = State::Today();
		if (const auto coming = g_coming.get()) {
			if (today - g_comingDay > 0.05f || Romance::GetStage(coming.get()) != Stage::kCommitted) {
				Off(coming.get());  // she never reached you (an hour of game time), or things changed: another time
				g_coming = {};
				logger::info("Confess: {} didn't reach you, another time", coming->GetDisplayFullName());
			}
			return;
		}
		if (!partner || g_said || Romance::GetStage(partner) != Stage::kCommitted) {
			return;
		}
		const auto candidate = Candidates::Find(partner);
		const auto& s = State::Of(partner->GetFormID());
		const auto& r = Rules::Get();
		if (!candidate || s.confessed || today - s.committedDay < r.confessDelayDays) {
			return;
		}
		if (s.pendingBreakup != Why::kNone || s.hurt || s.silentUntilDay > today || s.lockUntilDay > today || s.confrontPending) {
			return;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!partner->Is3DLoaded() || !World::SamePlace(player, partner) || player->GetPosition().GetDistance(partner->GetPosition()) > r.confessDistance) {
			return;
		}
		if (player->IsInCombat() || Busy(partner) || RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME) || !SafePlace(partner, *candidate)) {
			return;
		}
		const auto breakOff = [] {
			if (const auto coming = g_coming.get()) {
				Off(coming.get());  // another time
			}
			g_coming = {};
		};
		if (!Scenes::Begin(Scenes::Kind::kConfess, { partner }, breakOff)) {
			return;
		}
		const auto& f = Forms::Get();
		partner->AddToFaction(f.moment, static_cast<std::int8_t>(Rank(Moment::kConfess)));
		Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, f.sayQuest, partner, true);
		Romance::SyncChat(partner);
		g_coming = partner->GetHandle();
		g_comingDay = today;
		logger::info("Confess: {} comes to tell you how much you mean to her", partner->GetDisplayFullName());
	}

	void OnSaid(RE::Actor* a_partner)
	{
		State::Of(a_partner->GetFormID()).confessed = true;
		g_said = a_partner->GetHandle();
		g_coming = {};
		Scenes::Settle(Scenes::Kind::kConfess);  // the talk is on
		logger::info("Confess: {} told you", a_partner->GetDisplayFullName());
	}

	void OnDialogueClosed()
	{
		if (const auto said = g_said.get()) {
			Off(said.get());  // her answer to your reply needed the moment until now
		}
		g_said = {};
	}

	void OnGameLoaded()
	{
		g_coming = {};
		g_said = {};
	}
}
