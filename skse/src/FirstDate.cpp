#include "FirstDate.h"

#include "Candidates.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "Script.h"
#include "State.h"
#include "World.h"

namespace FirstDate
{
	using namespace IRF;

	namespace
	{
		// game thread (SKSE tasks)
		RE::ActorHandle g_date;      // the date is set with her (IRF_FirstDateQuest running)
		RE::ActorHandle g_greeting;  // she's coming up to you at her spot
		RE::ActorHandle g_on;        // she said "firstdate": the date ends with the talk

		// Her place for the journal: her add-on's firstDate.place, or the location of her spot's cell
		RE::BGSLocation* PlaceOf(const Candidates::Candidate& a_candidate)
		{
			if (a_candidate.attributes.datePlace) {
				return a_candidate.attributes.datePlace;
			}
			const auto spot = a_candidate.attributes.dateSpot;
			const auto cell = spot ? spot->GetParentCell() : nullptr;
			return cell ? cell->GetLocation() : nullptr;
		}

		void GreetOff(RE::Actor* a_actor)
		{
			Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_actor, false);
			if (a_actor->GetFactionRank(Forms::Get().moment, false) == Rank(Moment::kFirstDate)) {
				a_actor->AddToFaction(Forms::Get().moment, 0);
			}
		}

		// The date is over, kept or missed: the quest ends (its objective done or failed) and she goes back to her day
		void End(RE::Actor* a_actor, bool a_kept)
		{
			Script::CallStatic("IRF_Internal"sv, "EndDate"sv, Forms::Get().firstDateQuest, a_actor, a_kept);
			State::Of(a_actor->GetFormID()).dateUntilDay = 0.0f;
			GreetOff(a_actor);
			g_date = {};
			g_greeting = {};
		}

		bool Busy(RE::Actor* a_actor)
		{
			return a_actor->IsDead() || a_actor->IsInCombat() || RE::PlayerCharacter::GetSingleton()->IsInCombat() ||
			       RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
		}
	}

	void Start(RE::Actor* a_actor)
	{
		const auto candidate = Candidates::Find(a_actor);
		if (!candidate || !candidate->attributes.dateSpot || g_date) {
			return;
		}
		// tonight; asked after the evening is over, tomorrow night
		const auto calendar = RE::Calendar::GetSingleton();
		const float hour = calendar ? calendar->GetHour() : 12.0f;
		const float end = 23.5f;
		const float until = State::Today() + (hour < end ? end - hour : 24.0f - hour + end) / 24.0f;
		State::Of(a_actor->GetFormID()).dateUntilDay = until;
		g_date = a_actor->GetHandle();
		Script::CallStatic("IRF_Internal"sv, "StartDate"sv, Forms::Get().firstDateQuest, a_actor, candidate->attributes.dateSpot, PlaceOf(*candidate));
		logger::info("FirstDate: {} will meet you at her spot after sunset (until day {:.2f})", a_actor->GetDisplayFullName(), until);
	}

	bool Pending()
	{
		return static_cast<bool>(g_date);
	}

	void Check()
	{
		const auto date = g_date.get();
		if (!date) {
			return;
		}
		const auto  actor = date.get();
		const auto  candidate = Candidates::Find(actor);
		const auto& s = State::Of(actor->GetFormID());
		const auto  today = State::Today();
		if (!candidate || actor->IsDead() || Romance::GetStage(actor) != Stage::kFriend) {
			End(actor, false);  // things changed: no date, nothing owed
			logger::info("FirstDate: the date with {} is off", actor->GetDisplayFullName());
			return;
		}
		if (today > s.dateUntilDay && !g_on) {
			End(actor, false);
			Romance::ModApproval(actor, -Rules::Get().standUpCost, "stood up");
			Romance::Notify(actor, Event::kStoodUp);  // her line at your next meeting
			logger::info("FirstDate: you stood {} up", actor->GetDisplayFullName());
			return;
		}
		if (g_greeting || !World::Evening() || !actor->Is3DLoaded() || Busy(actor)) {
			return;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto spot = candidate->attributes.dateSpot;
		const float close = Rules::Get().firstDateDistance;
		if (!World::SamePlace(player, actor) || player->GetPosition().GetDistance(actor->GetPosition()) > close ||
			(spot && actor->GetPosition().GetDistance(spot->GetPosition()) > close)) {
			return;  // not both there yet
		}
		actor->AddToFaction(Forms::Get().moment, static_cast<std::int8_t>(Rank(Moment::kFirstDate)));
		Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, actor, true);
		g_greeting = actor->GetHandle();
		logger::info("FirstDate: you came; {} comes over", actor->GetDisplayFullName());
	}

	void OnSaid(RE::Actor* a_actor)
	{
		if (g_on) {
			return;
		}
		// she's said it: her come-over package goes now, or it greets you again as the talk closes (seen 2026-10-25);
		// the moment stays for her answer to your reply
		Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_actor, false);
		g_greeting = {};
		g_on = a_actor->GetHandle();
		Romance::Strayed(a_actor, "a date with someone else");  // with a partner it's straying
		Romance::Contact(a_actor);
	}

	void OnDialogueClosed()
	{
		const auto on = g_on.get();
		g_on = {};
		if (!on) {
			return;
		}
		const auto actor = on.get();
		auto&      s = State::Of(actor->GetFormID());
		End(actor, true);
		World::Kiss(actor);  // the date ends in a kiss, wherever it is (modesty doesn't apply)
		s.lastDateDay = State::Today();
		s.lastKissDay = State::Today();
		Romance::SetStage(actor, Stage::kDating, "the first date");
		World::SayMoment(actor, Moment::kFirstDateAfter, 2.0f);
		logger::info("FirstDate: the date with {} ended in a kiss", actor->GetDisplayFullName());
	}

	void OnGameLoaded()
	{
		g_date = {};
		g_greeting = {};
		g_on = {};
		for (const auto& candidate : Candidates::All()) {
			const auto actor = Candidates::ActorOf(candidate);
			if (actor && State::Of(actor->GetFormID()).dateUntilDay > 0.0f) {
				g_date = actor->GetHandle();  // the quest and its aliases come back with the save
				break;
			}
		}
	}
}
