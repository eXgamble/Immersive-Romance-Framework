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
		// the date's steps (IRF_DatePhase holds it: her follow package reads it, and it comes back with a save)
		enum class Phase : int
		{
			kWait = 0,  // set: she goes to her spot from sunset and waits
			kTalk = 1,  // her greeting at her spot: the talk's options, then the fade
			kAsk = 2,   // after the fade: "Walk me home?"
			kWalk = 3,  // she follows you home
			kDoor = 4,  // at her door: "(Kiss her.)" or "Goodnight."
		};

		// game thread (SKSE tasks)
		RE::ActorHandle g_date;              // the date is set with her (IRF_FirstDateQuest running)
		RE::ActorHandle g_on;                // she said "firstdate": the talk is on
		bool            g_sent{ false };     // the date's evening has come: she was sent to her spot
		bool            g_fading{ false };   // the talk is over: the fade (Papyrus) is on
		bool            g_asked{ false };    // she said "Walk me home?"
		bool            g_atDoor{ false };   // she said her line at her door
		bool            g_kiss{ false };     // ... and you chose "(Kiss her.)"
		bool            g_turning{ false };  // you strayed: walked back toward her (IRF_Internal.TurnBack)
		bool            g_turnStopped{ false };
		std::chrono::steady_clock::time_point g_turnStart;
		std::chrono::steady_clock::time_point g_turnEnd;  // the last turn back ended: the next waits kTurnGap
		constexpr float                       kTurnGap = 10.0f;  // seconds (a walk back that can't start ends at once)
		int              g_apart{ 0 };  // checks in a row she wasn't where you are (a door she didn't follow through)
		std::atomic<int> g_walk{ 0 };   // the walk's check (IRF_Internal.WalkHome) that's current; read by the native

		constexpr float kFrom = 18.0f;      // the date's evening: from sunset ...
		constexpr float kEnd = 23.5f;       // ... until it's late (then you stood her up)
		constexpr float kTomorrow = 17.0f;  // asked from here on, it's tomorrow night: she'd have to leave for her spot soon

		Phase PhaseNow()
		{
			const auto global = Forms::Get().datePhase;
			return global ? static_cast<Phase>(static_cast<int>(global->value)) : Phase::kWait;
		}

		void SetGlobal(RE::TESGlobal* a_global, float a_value)
		{
			if (a_global) {
				a_global->value = a_value;
			}
		}

		void SetPhase(Phase a_phase)
		{
			SetGlobal(Forms::Get().datePhase, static_cast<float>(a_phase));
		}

		// the day (GameDaysPassed) the date's evening starts
		float FromDay(float a_untilDay)
		{
			return a_untilDay - (kEnd - kFrom) / 24.0f;
		}

		// IRF_FirstDateWait takes her to her spot from then (its condition reads IRF_DateFrom)
		void SyncFrom(float a_untilDay)
		{
			SetGlobal(Forms::Get().dateFrom, a_untilDay > 0.0f ? FromDay(a_untilDay) : 1.0e6f);
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

		// Her door from the outside: where her add-on's evening.home (her home's exit door) leads
		RE::TESObjectREFR* HomeDoor(const Candidates::Candidate& a_candidate)
		{
			const auto inside = a_candidate.attributes.eveningHome;
			const auto extra = inside ? inside->extraList.GetByType<RE::ExtraTeleport>() : nullptr;
			const auto data = extra ? extra->teleportData : nullptr;
			return data ? data->linkedDoor.get().get() : nullptr;
		}

		bool Has(const Candidates::Candidate& a_candidate, const std::string& a_key)
		{
			const auto it = a_candidate.lines.find(a_key);
			return it != a_candidate.lines.end() && !it->second.empty();
		}

		std::int32_t MomentOf(RE::Actor* a_actor)
		{
			return a_actor->GetFactionRank(Forms::Get().moment, false);
		}

		void SetMoment(RE::Actor* a_actor, Moment a_moment)
		{
			a_actor->AddToFaction(Forms::Get().moment, static_cast<std::int8_t>(Rank(a_moment)));
		}

		// her "firstdate" greeting: on while she's at her spot on the date's evening (moment kFirstDate, kept in the
		// save with her factions)
		bool IsReady(RE::Actor* a_actor)
		{
			return MomentOf(a_actor) == Rank(Moment::kFirstDate);
		}

		void Ready(RE::Actor* a_actor, bool a_ready)
		{
			if (a_ready) {
				SetMoment(a_actor, Moment::kFirstDate);
			} else if (IsReady(a_actor)) {
				a_actor->AddToFaction(Forms::Get().moment, 0);
			}
		}

		// she comes up to you with the line of this moment (IRF_SayQuest's force greet)
		void ComeUp(RE::Actor* a_actor, Moment a_moment)
		{
			SetMoment(a_actor, a_moment);
			Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_actor, true);
		}

		void GreetOff(RE::Actor* a_actor)
		{
			Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_actor, false);
		}

		void StopTurning()
		{
			if (g_turning) {
				Script::CallStatic("Game"sv, "SetPlayerAIDriven"sv, false);
			}
			g_turning = false;
			g_turnStopped = false;
		}

		// The date is over, kept or missed: the quest ends (its objective done or failed) and she goes back to her day
		void End(RE::Actor* a_actor, bool a_kept)
		{
			const auto phase = PhaseNow();
			Script::CallStatic("IRF_Internal"sv, "EndDate"sv, Forms::Get().firstDateQuest, a_actor, a_kept);
			State::Of(a_actor->GetFormID()).dateUntilDay = 0.0f;
			SyncFrom(0.0f);
			const auto moment = MomentOf(a_actor);
			if (moment == Rank(Moment::kFirstDate) || moment == Rank(Moment::kWalkHome) || moment == Rank(Moment::kDateDoor)) {
				a_actor->AddToFaction(Forms::Get().moment, 0);
			}
			if (phase == Phase::kAsk || phase == Phase::kDoor) {
				GreetOff(a_actor);
			}
			StopTurning();
			SetPhase(Phase::kWait);
			++g_walk;  // a walk's check still running ends itself
			g_date = {};
			g_on = {};
			g_sent = false;
			g_fading = false;
			g_asked = false;
			g_atDoor = false;
			g_kiss = false;
			Scenes::End(Scenes::Kind::kFirstDate);
		}

		bool Busy(RE::Actor* a_actor)
		{
			return a_actor->IsDead() || a_actor->IsInCombat() || RE::PlayerCharacter::GetSingleton()->IsInCombat() ||
			       RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
		}

		// The end: at her door (or after the fade, for an add-on without the walk or the door). A kiss makes her
		// Dating; a goodnight leaves her a Friend who'll say yes to "Will you be mine?" (user, 2026-10-27)
		void Finish(RE::Actor* a_actor, bool a_kiss)
		{
			auto& s = State::Of(a_actor->GetFormID());
			End(a_actor, true);
			if (a_kiss) {
				World::Kiss(a_actor);  // modesty doesn't apply
				s.lastKissDay = State::Today();
				Romance::SetStage(a_actor, Stage::kDating, "the first date");
				World::SayMoment(a_actor, Moment::kFirstDateAfter, 2.0f);
				logger::info("FirstDate: the date with {} ended in a kiss", a_actor->GetDisplayFullName());
			} else {
				s.dated = true;
				Romance::Sync(a_actor);
				logger::info("FirstDate: the date with {} ended in a goodnight (\"Will you be mine?\" next)", a_actor->GetDisplayFullName());
			}
		}

		// at her door: she stops you there (or right after the fade, when her add-on has no walk)
		void Door(RE::Actor* a_actor)
		{
			const auto candidate = Candidates::Find(a_actor);
			if (!candidate || !Has(*candidate, "date.door")) {
				Finish(a_actor, true);
				return;
			}
			SetPhase(Phase::kDoor);
			StopTurning();
			g_atDoor = false;
			g_kiss = false;
			ComeUp(a_actor, Moment::kDateDoor);
			logger::info("FirstDate: at {}'s door", a_actor->GetDisplayFullName());
		}

		void Walk(RE::Actor* a_actor)
		{
			const auto candidate = Candidates::Find(a_actor);
			const auto door = candidate ? HomeDoor(*candidate) : nullptr;
			if (!door) {
				Door(a_actor);
				return;
			}
			GreetOff(a_actor);
			a_actor->AddToFaction(Forms::Get().moment, 0);
			SetPhase(Phase::kWalk);
			g_apart = 0;
			Script::CallStatic("IRF_Internal"sv, "DateWalk"sv, Forms::Get().firstDateQuest, a_actor, door);
			Script::CallStatic("IRF_Internal"sv, "WalkHome"sv, static_cast<std::int32_t>(++g_walk));
			logger::info("FirstDate: walking {} home", a_actor->GetDisplayFullName());
		}

		// once a second while you walk her home: turned back if you stray, stopped at her door
		void WalkCheck()
		{
			const auto date = g_date.get();
			if (!date || PhaseNow() != Phase::kWalk) {
				return;
			}
			const auto  actor = date.get();
			const auto  candidate = Candidates::Find(actor);
			const auto  door = candidate ? HomeDoor(*candidate) : nullptr;
			const auto  player = RE::PlayerCharacter::GetSingleton();
			const auto& r = Rules::Get();
			if (!door || actor->IsDead()) {
				return;  // the next talk or place ends it (Check)
			}
			const bool  together = World::SamePlace(player, actor);
			const float apart = together ? player->GetPosition().GetDistance(actor->GetPosition()) : 1.0e9f;
			if (g_turning) {
				const auto secs = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_turnStart).count();
				if (!g_turnStopped && (apart < 300.0f || secs > 6.0f)) {
					Script::CallStatic("Game"sv, "SetPlayerAIDriven"sv, false);  // ends the walk back (it can stall)
					g_turnStopped = true;
				} else if (g_turnStopped && secs > 9.0f) {
					// it never returned: end it in place (as Fated Start does)
					Script::Call(player, "ObjectReference"sv, "MoveTo"sv, static_cast<RE::TESObjectREFR*>(player), 0.0f, 0.0f, 0.0f, false);
					g_turning = false;
					g_turnStopped = false;
					g_turnEnd = std::chrono::steady_clock::now();
				}
				return;
			}
			if (!together) {
				if (++g_apart >= 3) {  // through a door she didn't follow: she catches up
					Script::Call(actor, "ObjectReference"sv, "MoveTo"sv, static_cast<RE::TESObjectREFR*>(player), -80.0f, -80.0f, 0.0f, true);
					g_apart = 0;
				}
				return;
			}
			g_apart = 0;
			if (World::SamePlace(door, actor) && actor->GetPosition().GetDistance(door->GetPosition()) <= r.dateHomeDistance) {
				Door(actor);
				return;
			}
			const auto sinceTurn = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_turnEnd).count();
			if (apart > r.dateStrayDistance && !player->IsInCombat() && sinceTurn >= kTurnGap) {
				g_turning = true;
				g_turnStopped = false;
				g_turnStart = std::chrono::steady_clock::now();
				Script::CallStatic("IRF_Internal"sv, "TurnBack"sv, actor,
					RE::BSFixedString(std::format("I should walk {} home.", actor->GetDisplayFullName())));
				logger::info("FirstDate: you strayed from {}: turned back", actor->GetDisplayFullName());
			}
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
		SetPhase(Phase::kWait);
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
		if (PhaseNow() != Phase::kWait) {
			return;  // on the date: the talk, the walk and her door move it on
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
		// her greeting: the talk is on. Its options start fresh; the closing option shows after one of them (at once
		// for an add-on without them)
		g_on = a_actor->GetHandle();
		SetPhase(Phase::kTalk);
		for (const auto global : Forms::Get().dateAsked) {
			SetGlobal(global, 0.0f);
		}
		const auto candidate = Candidates::Find(a_actor);
		SetGlobal(Forms::Get().dateTalked, candidate && Has(*candidate, "date.talk.1") ? 0.0f : 1.0f);
		if (!Scenes::On(Scenes::Kind::kFirstDate)) {
			Scenes::Begin(Scenes::Kind::kFirstDate, { a_actor }, {});  // the stage is the date's until it's over
		}
		Scenes::Settle(Scenes::Kind::kFirstDate);  // the talk is on
		Romance::Strayed(a_actor, "a date with someone else");  // with a partner it's straying
		Romance::Contact(a_actor);
	}

	void OnLineBegan(RE::Actor* a_actor, const std::string& a_key)
	{
		if (!g_date || g_date.get().get() != a_actor) {
			return;
		}
		if (a_key.size() == 11 && a_key.starts_with("date.talk.")) {  // "date.talk.N": that option is used
			const int n = a_key[10] - '0';
			if (n >= 1 && n <= 5) {
				SetGlobal(Forms::Get().dateAsked[n - 1], 1.0f);
				SetGlobal(Forms::Get().dateTalked, 1.0f);
			}
		} else if (a_key == "date.walkhome") {
			GreetOff(a_actor);  // she's said it: the come-up package goes, or it greets you again as the talk closes
			g_asked = true;
		} else if (a_key == "date.door") {
			GreetOff(a_actor);
			g_atDoor = true;
		} else if (a_key == "date.kiss") {
			g_kiss = true;
		}
	}

	void OnDialogueClosed()
	{
		const auto date = g_date.get();
		if (!date) {
			return;
		}
		const auto actor = date.get();
		switch (PhaseNow()) {
		case Phase::kTalk:
			if (g_on && !g_fading) {
				// the talk is over (the closing option, or you left it): the fade, then "Walk me home?"
				g_fading = true;
				State::Of(actor->GetFormID()).lastDateDay = State::Today();
				Romance::Contact(actor);
				Script::CallStatic("IRF_Internal"sv, "DateFade"sv, actor, Rules::Get().eveningHours);
				logger::info("FirstDate: the talk with {} is over; the evening passes", actor->GetDisplayFullName());
			}
			break;
		case Phase::kAsk:
			if (g_asked) {
				Walk(actor);  // whatever you answered (or if you just left the talk), she follows you home
			}
			break;
		case Phase::kDoor:
			if (g_atDoor) {
				Finish(actor, g_kiss);  // leaving the talk without a choice is a goodnight
			}
			break;
		default:
			break;
		}
	}

	void Faded(RE::Actor* a_actor)
	{
		g_fading = false;
		if (!g_date || g_date.get().get() != a_actor || PhaseNow() != Phase::kTalk) {
			return;
		}
		g_on = {};
		a_actor->AddToFaction(Forms::Get().moment, 0);
		const auto candidate = Candidates::Find(a_actor);
		if (candidate && Has(*candidate, "date.walkhome") && HomeDoor(*candidate)) {
			SetPhase(Phase::kAsk);
			g_asked = false;
			ComeUp(a_actor, Moment::kWalkHome);
			logger::info("FirstDate: {} asks you to walk her home", a_actor->GetDisplayFullName());
		} else {
			Door(a_actor);  // no walk home in her add-on: her door scene (or the kiss) right here
		}
	}

	bool WalkTick(std::int32_t a_walk)
	{
		if (a_walk != g_walk.load() || PhaseNow() != Phase::kWalk) {
			return false;
		}
		SKSE::GetTaskInterface()->AddTask([]() { WalkCheck(); });
		return true;
	}

	void TurnedBack()
	{
		g_turning = false;
		g_turnStopped = false;
		g_turnEnd = std::chrono::steady_clock::now();
	}

	bool Blocking()
	{
		return g_date && PhaseNow() != Phase::kWait;
	}

	std::string BlockMessage()
	{
		const auto date = g_date.get();
		return std::format("I should walk {} home first.", date ? date->GetDisplayFullName() : "her");
	}

	void OnGameLoaded()
	{
		g_date = {};
		g_on = {};
		g_sent = false;
		g_fading = false;
		g_turning = false;
		g_turnStopped = false;
		g_turnEnd = {};
		g_asked = false;
		g_atDoor = false;
		g_kiss = false;
		++g_walk;  // the save's walk check (if any) ends itself; Resume starts a new one
	}

	void Resume()
	{
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
		const auto date = g_date.get();
		const auto phase = PhaseNow();
		if (!date) {
			SetPhase(Phase::kWait);
			return;
		}
		// the saved step carries on: "Walk me home?" was said once its moment is gone; the walk's check starts again
		// (a newer one: the one in the save ends itself); at her door she comes up to you again
		g_asked = phase == Phase::kAsk && MomentOf(date.get()) != Rank(Moment::kWalkHome);
		if (phase == Phase::kTalk) {
			g_on = date->GetHandle();
			Faded(date.get());  // saved in the fade: on to the next step
		} else if (phase == Phase::kWalk) {
			Script::CallStatic("IRF_Internal"sv, "WalkHome"sv, static_cast<std::int32_t>(++g_walk));
		} else if (phase == Phase::kDoor) {
			Door(date.get());
		}
	}
}
