#include "Expose.h"

#include "Family.h"

#include "Candidates.h"
#include "Follower.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "Script.h"
#include "State.h"
#include "World.h"

namespace Expose
{
	using namespace IRF;

	namespace
	{
		enum class Step
		{
			kNone,
			kWalk,      // A walks up to B
			kAsk,       // A: "Who are you to them?"
			kClaim,     // B claims you
			kCounter,   // A answers
			kQuestion,  // B comes up to you and asks; your answer
			kOutcome,   // A's line after the talk
		};

		enum class Answer
		{
			kNone,
			kSideA,  // "I don't know who you are. Let's get out of here, <A>." (B is disowned; A must believe you)
			kSideB,  // "It's not true. I don't even know this person." (A is disowned; B must believe you)
			kTrue,   // "It's true." (or walking away from the question)
		};

		// IRF_ExposeFaction on the one saying an outcome line
		constexpr int kBelieved = 1;
		constexpr int kDisbelieved = 2;
		constexpr int kDisowned = 3;
		constexpr int kTrueRank = 4;

		// game thread (SKSE tasks)
		RE::ActorHandle g_a{};
		RE::ActorHandle g_b{};
		Step            g_step{ Step::kNone };
		float           g_stepDay{ 0.0f };
		bool            g_persuaded{ false };  // your Speech carries the lie, whoever you lie to
		Answer          g_answer{ Answer::kNone };
		bool            g_begun{ false };  // this step's line has started
		std::string     g_sideAText;       // your first answer as the plugin has it, with its name token
		int             g_retries{ 0 };

		const char* SetName(Stage a_stage)
		{
			switch (a_stage) {
			case Stage::kDating:
				return "dating";
			case Stage::kCommitted:
				return "committed";
			case Stage::kEngaged:
				return "engaged";
			case Stage::kMarried:
				return "spouse";
			default:
				return "";
			}
		}

		bool Has(const Candidates::Candidate* a_candidate, const std::string& a_key)
		{
			return a_candidate && a_candidate->lines.contains(a_key);
		}

		// both add-ons have what their roles need
		bool HasLines(RE::Actor* a_b, RE::Actor* a_a)
		{
			const auto b = Candidates::Find(a_b);
			const auto a = Candidates::Find(a_a);
			const auto bSet = SetName(Romance::GetStage(a_b));
			const auto aSet = SetName(Romance::GetStage(a_a));
			for (const auto key : { "expose.ask"s, std::format("expose.counter.{}", aSet), "expose.believed"s, "expose.disbelieved"s,
					 "expose.disowned"s, "expose.true"s }) {
				if (!Has(a, key)) {
					logger::info("Expose: {} has no '{}': no scene", a_a->GetDisplayFullName(), key);
					return false;
				}
			}
			for (const auto key : { std::format("expose.claim.{}", bSet), "expose.question"s, "expose.reply.disowned"s,
					 "expose.reply.believed"s, "expose.reply.disbelieved"s, "expose.reply.true"s }) {
				if (!Has(b, key)) {
					logger::info("Expose: {} has no '{}': no scene", a_b->GetDisplayFullName(), key);
					return false;
				}
			}
			return true;
		}

		void Go(Step a_step)
		{
			g_step = a_step;
			g_stepDay = State::Today();
			g_begun = false;
			g_retries = 0;
		}

		// the line this step waits for: who says it, which moment
		bool StepLine(RE::Actor*& a_speaker, Moment& a_moment)
		{
			switch (g_step) {
			case Step::kAsk:
				a_speaker = g_a.get().get();
				a_moment = Moment::kExposeAsk;
				return true;
			case Step::kClaim:
				a_speaker = g_b.get().get();
				a_moment = Moment::kExposeClaim;
				return true;
			case Step::kCounter:
				a_speaker = g_a.get().get();
				a_moment = Moment::kExposeCounter;
				return true;
			case Step::kOutcome:
				a_speaker = g_a.get().get();
				a_moment = Moment::kExposeOutcome;
				return true;
			default:
				return false;
			}
		}

		void Clear()
		{
			const auto& f = Forms::Get();
			const auto  a = g_a.get();
			const auto  b = g_b.get();
			for (const auto actor : { a.get(), b.get() }) {
				if (actor) {
					actor->AddToFaction(f.expose, 0);
					if (actor->GetFactionRank(f.moment, false) > 0) {
						actor->AddToFaction(f.moment, 0);
					}
				}
			}
			if (a && b) {
				Script::CallStatic("IRF_Internal"sv, "Release"sv, f.sayQuest, a.get(), b.get());
			}
			if (b) {
				Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, f.sayQuest, b.get(), false);
			}
			g_a = {};
			g_b = {};
			g_step = Step::kNone;
			g_answer = Answer::kNone;
		}

		// she never forgives being denied to her face, unless she's your wife (no divorce): then her heart is broken
		// for three times as long (user, 2026-10-26)
		void Disown(RE::Actor* a_actor)
		{
			if (Romance::GetStage(a_actor) == Stage::kMarried) {
				Romance::Heartbroken(a_actor, "you denied her", Rules::Get().disownedSpouseMult);
			} else {
				Romance::BreakUp(a_actor, Who::kThem, Why::kCheating, true);
			}
		}

		// she leaves you over it, and can be won back as usual; a wife can't leave: her heart is broken
		void Leave(RE::Actor* a_actor)
		{
			if (Romance::GetStage(a_actor) == Stage::kMarried) {
				Romance::Heartbroken(a_actor, "it came out", 1);
			} else {
				Romance::BreakUp(a_actor, Who::kThem, Why::kCheating);
			}
		}

		void Finish()
		{
			const auto a = g_a.get();
			const auto b = g_b.get();
			const auto answer = g_answer;
			const bool persuaded = g_persuaded;
			Clear();
			if (!a || !b) {
				return;
			}
			State::Of(a->GetFormID()).knowSeeing.insert(b->GetFormID());  // each knows of the other now
			State::Of(b->GetFormID()).knowSeeing.insert(a->GetFormID());
			const auto cost = Rules::Get().exposeDoubtCost;
			switch (answer) {
			case Answer::kSideA:
				logger::info("Expose: you sided with {} ({})", a->GetDisplayFullName(), persuaded ? "she believed you" : "she didn't believe you");
				Disown(b.get());
				if (persuaded) {
					Romance::ModApproval(a.get(), -cost, "a doubt she keeps", Why::kCheating);
				} else {
					Leave(a.get());
				}
				break;
			case Answer::kSideB:
				logger::info("Expose: you sided with {} ({})", b->GetDisplayFullName(), persuaded ? "she believed you" : "she didn't believe you");
				Disown(a.get());
				if (persuaded) {
					Romance::ModApproval(b.get(), -cost, "a doubt she keeps", Why::kCheating);
				} else {
					Leave(b.get());
				}
				break;
			default:
				logger::info("Expose: you told them it's true");
				Leave(a.get());
				Leave(b.get());
				break;
			}
		}

		// something broke it off: both simply catch you
		void Abort(std::string_view a_why)
		{
			const auto a = g_a.get();
			const auto b = g_b.get();
			logger::info("Expose: broken off ({}): both catch you", a_why);
			Clear();
			if (a && b && Romance::IsPartner(a.get()) && Romance::IsPartner(b.get())) {
				Romance::Caught(a.get(), "it came out", 1.5f, b.get());
				Romance::Caught(b.get(), "it came out", 1.5f + Rules::Get().caughtLineGap, a.get());
			}
		}

		bool Fine(RE::Actor* a_actor)
		{
			return a_actor && !a_actor->IsDead() && World::SamePlace(a_actor, RE::PlayerCharacter::GetSingleton()) && !a_actor->IsInCombat();
		}
	}

	bool Running()
	{
		return g_step != Step::kNone;
	}

	bool Start(RE::Actor* a_b, RE::Actor* a_a)
	{
		if (Running() || Family::Running()) {
			logger::info("Expose: a scene is already on");
			return false;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		for (const auto actor : { a_a, a_b }) {
			if (!Fine(actor)) {
				logger::info("Expose: not now ({}: dead {}, here {}, fighting {})", actor->GetDisplayFullName(), actor->IsDead(),
					World::SamePlace(actor, player), actor->IsInCombat());
				return false;
			}
		}
		if (player->IsInCombat()) {
			logger::info("Expose: not now (you're in a fight)");
			return false;
		}
		if (!HasLines(a_b, a_a)) {
			return false;
		}
		const auto& f = Forms::Get();
		const auto  speech = RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeech);
		g_persuaded = speech >= static_cast<float>(Rules::Get().exposePersuade);
		g_a = a_a->GetHandle();
		g_b = a_b->GetHandle();
		g_answer = Answer::kNone;
		Follower::Dismiss(a_a);  // the scene needs them where they stand (they won't follow you after it anyway)
		Follower::Dismiss(a_b);
		a_b->AddToFaction(f.expose, g_persuaded ? kBelieved : kDisbelieved);  // her answer if you side with her
		// your first answer names A: the game doesn't fill an alias filled mid-game into a topic's text, so the token
		// the plugin's text carries is replaced here (translations keep the token wherever their sentence needs it)
		if (g_sideAText.empty()) {
			g_sideAText = f.exposeSideA->GetFullName();
		}
		auto text = g_sideAText;
		if (const auto at = text.find("<Alias=ExposeA>"); at != std::string::npos) {
			text.replace(at, std::strlen("<Alias=ExposeA>"), a_a->GetDisplayFullName());
		}
		f.exposeSideA->fullName = text;
		logger::info("Expose: {} and {} have it out (a lie would {})", a_a->GetDisplayFullName(), a_b->GetDisplayFullName(),
			g_persuaded ? "work" : "fail");
		Go(Step::kWalk);
		Script::CallStatic("IRF_Internal"sv, "Approach"sv, f.sayQuest, a_a, a_b, 1);  // A walks over; OnArrived goes on from there
		return true;
	}

	void OnArrived()
	{
		if (g_step != Step::kWalk) {
			return;
		}
		if (const auto a = g_a.get()) {
			Go(Step::kAsk);
			World::SayMoment(a.get(), Moment::kExposeAsk, 0.5f);
		}
	}

	void OnLineBegan(RE::Actor* a_speaker, const std::string& a_key)
	{
		RE::Actor* speaker = nullptr;
		Moment     moment{};
		if (Running() && a_key.starts_with("expose.") && StepLine(speaker, moment) && speaker == a_speaker) {
			g_begun = true;
		}
	}

	void OnLineEnded(RE::Actor* a_speaker, const std::string& a_key)
	{
		const auto a = g_a.get();
		const auto b = g_b.get();
		if (!Running() || !a || !b) {
			return;
		}
		if (g_step == Step::kAsk && a_speaker == a.get() && a_key == "expose.ask") {
			Go(Step::kClaim);
			World::SayMoment(b.get(), Moment::kExposeClaim, 0.3f);
		} else if (g_step == Step::kClaim && a_speaker == b.get() && a_key.starts_with("expose.claim.")) {
			Go(Step::kCounter);
			World::SayMoment(a.get(), Moment::kExposeCounter, 0.3f);
		} else if (g_step == Step::kCounter && a_speaker == a.get() && a_key.starts_with("expose.counter.")) {
			Go(Step::kQuestion);
			const auto& f = Forms::Get();
			a->AddToFaction(f.moment, 0);
			b->AddToFaction(f.moment, static_cast<std::int8_t>(Rank(Moment::kExposeQuestion)));
			Script::CallStatic("IRF_Internal"sv, "Hold"sv, f.sayQuest, 2, static_cast<RE::Actor*>(nullptr));  // she may move
			Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, f.sayQuest, b.get(), true);  // she comes up to you and asks
		} else if (g_step == Step::kOutcome && a_speaker == a.get() &&
				   (a_key == "expose.believed" || a_key == "expose.disbelieved" || a_key == "expose.disowned" || a_key == "expose.true")) {
			Finish();
		}
	}

	void OnAnswered(RE::Actor* a_speaker, const std::string& a_key)
	{
		if (g_step != Step::kQuestion || a_speaker != g_b.get().get()) {
			return;
		}
		g_answer = a_key == "expose.reply.disowned" ? Answer::kSideA :
		           a_key == "expose.reply.true"     ? Answer::kTrue :
		                                              Answer::kSideB;  // believed / disbelieved
		// her question is answered: it mustn't come up again as the talk closes (its moment goes, so a second force
		// greet finds nothing to say), and she stays where she is for A's last word
		a_speaker->AddToFaction(Forms::Get().moment, 0);
		Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_speaker, false);
		Script::CallStatic("IRF_Internal"sv, "Hold"sv, Forms::Get().sayQuest, 2, a_speaker);
	}

	void OnDialogueClosed()
	{
		if (g_step != Step::kQuestion) {
			return;
		}
		const auto a = g_a.get();
		const auto b = g_b.get();
		if (!a || !b) {
			Abort("someone is gone");
			return;
		}
		if (g_answer == Answer::kNone) {
			g_answer = Answer::kTrue;  // walking away from the question says enough
		}
		const auto& f = Forms::Get();
		b->AddToFaction(f.moment, 0);
		Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, f.sayQuest, b.get(), false);  // walked away: no second asking
		Script::CallStatic("IRF_Internal"sv, "Hold"sv, f.sayQuest, 2, b.get());
		const int rank = g_answer == Answer::kSideA ? (g_persuaded ? kBelieved : kDisbelieved) :
		                 g_answer == Answer::kSideB ? kDisowned :
		                                              kTrueRank;
		a->AddToFaction(f.expose, static_cast<std::int8_t>(rank));
		Go(Step::kOutcome);
		World::SayMoment(a.get(), Moment::kExposeOutcome, 1.5f);  // a line within ~1 s of the talk closing never plays
	}

	void Check()
	{
		if (!Running()) {
			return;
		}
		const auto a = g_a.get();
		const auto b = g_b.get();
		const auto player = RE::PlayerCharacter::GetSingleton();
		for (const auto actor : { a.get(), b.get() }) {
			if (g_step == Step::kOutcome && actor == b.get()) {
				continue;  // she has had her say: only A's last word is left
			}
			// gone: dead, or no longer where you are (a door, a fast travel); not "3D loaded", which flickers
			if (!actor || actor->IsDead() || !World::SamePlace(actor, player)) {
				Abort(std::format("{} is gone", actor ? actor->GetDisplayFullName() : "someone"));
				return;
			}
			if (actor->IsInCombat()) {
				Abort(std::format("{} is in a fight", actor->GetDisplayFullName()));
				return;
			}
		}
		if (player->IsInCombat()) {
			Abort("you're in a fight");
			return;
		}
		if (player->GetPosition().GetDistance(b->GetPosition()) > Rules::Get().exposeLeaveDistance) {
			Abort("you walked off");
			return;
		}
		// a line that didn't start (she was busy saying something else): said again, twice at most
		RE::Actor* speaker = nullptr;
		Moment     moment{};
		if (!g_begun && StepLine(speaker, moment) && speaker && State::Today() - g_stepDay > 0.002f) {  // ~3 game minutes
			if (g_retries >= 2) {
				Abort("a line never played");
				return;
			}
			++g_retries;
			g_stepDay = State::Today();
			logger::info("Expose: {}'s line didn't start; again", speaker->GetDisplayFullName());
			World::SayMoment(speaker, moment, 0.3f);
			return;
		}
		if (State::Today() - g_stepDay > 0.03f) {
			Abort("it stalled");  // about 45 minutes of game time on one step: a path never walked, a talk never opened
		}
	}

	void OnGameLoaded()
	{
		// a scene in a save isn't picked up again: whatever it left on them goes (the scene itself is forgotten)
		const auto& f = Forms::Get();
		for (const auto& candidate : Candidates::All()) {
			if (const auto actor = Candidates::ActorOf(candidate); actor && actor->GetFactionRank(f.expose, false) > 0) {
				actor->AddToFaction(f.expose, 0);
			}
		}
		g_a = {};
		g_b = {};
		g_step = Step::kNone;
		g_answer = Answer::kNone;
	}
}
