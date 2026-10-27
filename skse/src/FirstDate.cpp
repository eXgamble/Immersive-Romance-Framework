#include "FirstDate.h"

#include "Candidates.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "Scenes.h"
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
		RE::ActorHandle g_on;        // she said "firstdate": the date ends with the talk
		bool            g_sent{ false };  // the date's evening has come: she was sent to her spot

		constexpr float kFrom = 18.0f;  // the date's evening: from sunset ...
		constexpr float kEnd = 23.5f;   // ... until it's late (then you stood her up)
		constexpr float kTomorrow = 17.0f;  // asked from here on, it's tomorrow night: she'd have to leave for her spot soon

		// the day (GameDaysPassed) the date's evening starts
		float FromDay(float a_untilDay)
		{
			return a_untilDay - (kEnd - kFrom) / 24.0f;
		}

		// IRF_FirstDateWait takes her to her spot from then (its condition reads IRF_DateFrom)
		void SyncFrom(float a_untilDay)
		{
			if (const auto global = Forms::Get().dateFrom) {
				global->value = a_untilDay > 0.0f ? FromDay(a_untilDay) : 1.0e6f;
			}
		}

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

		// her "firstdate" greeting: on while she's at her spot on the date's evening (moment kFirstDate, kept in the
		// save with her factions)
		bool IsReady(RE::Actor* a_actor)
		{
			return a_actor->GetFactionRank(Forms::Get().moment, false) == Rank(Moment::kFirstDate);
		}

		void Ready(RE::Actor* a_actor, bool a_ready)
		{
			if (a_ready) {
				a_actor->AddToFaction(Forms::Get().moment, static_cast<std::int8_t>(Rank(Moment::kFirstDate)));
			} else if (IsReady(a_actor)) {
				a_actor->AddToFaction(Forms::Get().moment, 0);
			}
		}

		// The date is over, kept or missed: the quest ends (its objective done or failed) and she goes back to her day
		void End(RE::Actor* a_actor, bool a_kept)
		{
			Script::CallStatic("IRF_Internal"sv, "EndDate"sv, Forms::Get().firstDateQuest, a_actor, a_kept);
			State::Of(a_actor->GetFormID()).dateUntilDay = 0.0f;
			SyncFrom(0.0f);
			Ready(a_actor, false);
			g_date = {};
			g_sent = false;
			Scenes::End(Scenes::Kind::kFirstDate);
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
		// tonight; asked from 5 pm (user, 2026-10-27), tomorrow night
		const auto  calendar = RE::Calendar::GetSingleton();
		const float hour = calendar ? calendar->GetHour() : 12.0f;
		const bool  tomorrow = hour >= kTomorrow;
		const float until = State::Today() + (kEnd - hour + (tomorrow ? 24.0f : 0.0f)) / 24.0f;
		State::Of(a_actor->GetFormID()).dateUntilDay = until;
		SyncFrom(until);
		g_date = a_actor->GetHandle();
		g_sent = false;
		Script::CallStatic("IRF_Internal"sv, "StartDate"sv, Forms::Get().firstDateQuest, a_actor, candidate->attributes.dateSpot, PlaceOf(*candidate));
		logger::info("FirstDate: {} will meet you at her spot {} after sunset (until day {:.2f})", a_actor->GetDisplayFullName(),
			tomorrow ? "tomorrow" : "tonight", until);
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
		SyncFrom(s.dateUntilDay);  // a skipped day (IRF - Debug) moves it too
		if (today < FromDay(s.dateUntilDay)) {
			return;  // not the date's evening yet
		}
		if (!g_sent) {
			g_sent = true;
			Script::Call(actor, "Actor"sv, "EvaluatePackage"sv);  // her wait package is on now: off to her spot
		}
		if (g_on || Busy(actor)) {
			return;  // the date's talk is on, or not now (her greeting stays as it is)
		}
		// at her spot after sunset, her greeting is the date: you go up to her and talk (user, 2026-10-27: no force
		// greet). Looking at her runs this check, so it's on before any talk with her, however early you came
		const auto  spot = candidate->attributes.dateSpot;
		const float close = Rules::Get().firstDateDistance;
		const bool  there = World::Evening() && actor->Is3DLoaded() && spot && World::SamePlace(spot, actor) &&
		                   actor->GetPosition().GetDistance(spot->GetPosition()) <= close;
		if (there != IsReady(actor)) {
			Ready(actor, there);
			if (there) {
				logger::info("FirstDate: {} waits at her spot; talk to her for the date", actor->GetDisplayFullName());
			}
		}
	}

	void OnSaid(RE::Actor* a_actor)
	{
		if (g_on) {
			return;
		}
		// she's said it: the moment stays for her answer to your reply
		g_on = a_actor->GetHandle();
		if (!Scenes::On(Scenes::Kind::kFirstDate)) {
			Scenes::Begin(Scenes::Kind::kFirstDate, { a_actor }, {});  // the stage is the date's until the talk ends
		}
		Scenes::Settle(Scenes::Kind::kFirstDate);  // the talk is on
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
		g_on = {};
		g_sent = false;
		float until = 0.0f;
		for (const auto& candidate : Candidates::All()) {
			const auto actor = Candidates::ActorOf(candidate);
			if (actor && State::Of(actor->GetFormID()).dateUntilDay > 0.0f) {
				g_date = actor->GetHandle();  // the quest and its aliases come back with the save
				until = State::Of(actor->GetFormID()).dateUntilDay;
				break;
			}
		}
		SyncFrom(until);
	}
}
