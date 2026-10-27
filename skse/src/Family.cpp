#include "Family.h"

#include "Candidates.h"
#include "Children.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "Scenes.h"
#include "Script.h"
#include "State.h"
#include "World.h"

namespace Family
{
	using namespace IRF;

	namespace
	{
		// game thread (SKSE tasks)
		RE::ActorHandle                         g_her{};
		RE::ActorHandle                         g_child{};
		const Candidates::FamilyTalk*           g_talk{ nullptr };
		int                                     g_number{ 0 };    // the conversation's number (1-based)
		int                                     g_step{ -1 };     // the line playing (0-based); -1 walking over
		float                                   g_stepDay{ 0.0f };
		bool                                    g_begun{ false };
		bool                                    g_waitTalk{ false };  // you're talking to someone: the line waits for it to end
		int                                     g_retries{ 0 };
		float                                   g_lastDay{ -1.0f };  // one conversation a game day
		std::map<std::pair<RE::FormID, int>, float> g_played;       // (her, number) -> when: the least recent plays next

		Home HomeOf(RE::Actor* a_her)
		{
			const auto& s = State::Of(a_her->GetFormID());
			if (s.hurt || s.silentUntilDay > State::Today()) {
				return Home::kHeartbroken;
			}
			const int band = Romance::Band(Romance::GetApproval(a_her));  // as "How are we?"
			return band >= 4 ? Home::kHappy : band == 3 ? Home::kFine : band == 2 ? Home::kStrained : Home::kBad;
		}

		RE::Actor* Speaker(int a_step)
		{
			if (!g_talk || a_step < 0 || a_step >= static_cast<int>(g_talk->steps.size())) {
				return nullptr;
			}
			return (g_talk->steps[a_step].first ? g_child : g_her).get().get();
		}

		void Say(int a_step)
		{
			g_step = a_step;
			g_stepDay = State::Today();
			g_begun = false;
			g_retries = 0;
			if (const auto speaker = Speaker(a_step)) {
				speaker->AddToFaction(Forms::Get().familyStep, static_cast<std::int8_t>(a_step + 1));
				World::SayMoment(speaker, Moment::kFamily, 0.3f);
			}
		}

		void End(std::string_view a_why)
		{
			const auto& f = Forms::Get();
			const auto  her = g_her.get();
			const auto  child = g_child.get();
			for (const auto actor : { her.get(), child.get() }) {
				if (actor) {
					actor->AddToFaction(f.familyStep, 0);
					if (actor->GetFactionRank(f.moment, false) == Rank(Moment::kFamily)) {
						actor->AddToFaction(f.moment, 0);
					}
				}
			}
			if (her) {
				her->AddToFaction(f.family, 0);
			}
			if (her && child) {
				Script::CallStatic("IRF_Internal"sv, "Release"sv, f.sayQuest, her.get(), child.get());
			}
			logger::info("Family: conversation {} over ({})", g_number, a_why);
			g_her = {};
			g_child = {};
			g_talk = nullptr;
			g_number = 0;
			g_step = -1;
			Scenes::End(Scenes::Kind::kFamily);
		}

		bool Here(RE::Actor* a_actor, RE::Actor* a_near, float a_distance)
		{
			return a_actor && !a_actor->IsDead() && a_actor->Is3DLoaded() && !a_actor->IsInCombat() && World::SamePlace(a_actor, a_near) &&
			       a_actor->GetPosition().GetDistance(a_near->GetPosition()) < a_distance;
		}
	}

	bool Running()
	{
		return g_talk != nullptr;
	}

	void Check()
	{
		const auto today = State::Today();
		if (Running()) {
			Tick();  // a look, a place, a closed talk: one going on may have to end (its walker never arrived)
			return;
		}
		if (!Rules::Get().familyTalks) {
			return;
		}
		if (std::floor(today) == std::floor(g_lastDay)) {
			return;  // cheap first: one a day
		}
		const auto her = Romance::Partner();
		const auto candidate = her ? Candidates::Find(her) : nullptr;
		if (!candidate || candidate->family.empty() || Romance::GetStage(her) != Stage::kMarried) {
			return;
		}
		const auto  player = RE::PlayerCharacter::GetSingleton();
		const auto& r = Rules::Get();
		if (player->IsInCombat() || !Here(her, player, r.familyHearDistance) || RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
			return;
		}
		// the children close to her
		std::vector<RE::Actor*> boys;
		std::vector<RE::Actor*> girls;
		for (const auto& [ref, data] : State::Children()) {
			const auto child = RE::TESForm::LookupByID<RE::Actor>(ref);
			if (child && Children::IsChild(child) && Here(child, her, r.familyHearDistance)) {
				const auto base = child->GetActorBase();
				(base && base->IsFemale() ? girls : boys).push_back(child);
			}
		}
		if (boys.empty() && girls.empty()) {
			return;
		}
		// the conversations that fit how things are at home, her mood and the children there; the least recent wins
		const auto home = HomeOf(her);
		const int  mood = her->IsInFaction(Forms::Get().moodFaction) ? her->GetFactionRank(Forms::Get().moodFaction, false) : 0;
		int        best = 0;
		float      bestDay = std::numeric_limits<float>::max();
		for (int i = 0; i < static_cast<int>(candidate->family.size()); ++i) {
			const auto& t = candidate->family[i];
			if (!(t.homes & (1u << Rank(home))) || (t.mood >= 0 && t.mood != mood) || (t.child == 1 && boys.empty()) || (t.child == 2 && girls.empty())) {
				continue;
			}
			const auto living = [](RE::FormID a_ref) {
				const auto who = RE::TESForm::LookupByID<RE::Actor>(a_ref);
				return who && !who->IsDead();
			};
			if ((t.alive && !living(t.alive)) || (t.dead && living(t.dead))) {
				continue;  // about someone who isn't (or is still) alive
			}
			const auto it = g_played.find({ her->GetFormID(), i + 1 });
			const float day = it == g_played.end() ? -1.0f + static_cast<float>(std::rand() % 100) / 1000.0f : it->second;  // skse-lint: ok a tie-break, once a day
			if (day < bestDay) {
				bestDay = day;
				best = i + 1;
			}
		}
		if (best == 0) {
			return;
		}
		const auto& talk = candidate->family[best - 1];
		std::vector<RE::Actor*> pool;
		if (talk.child != 2) {
			pool.insert(pool.end(), boys.begin(), boys.end());
		}
		if (talk.child != 1) {
			pool.insert(pool.end(), girls.begin(), girls.end());
		}
		const auto child = pool[static_cast<std::size_t>(std::rand()) % pool.size()];  // skse-lint: ok once a day
		if (!Scenes::Begin(Scenes::Kind::kFamily, { her, child }, [] { End("another scene needs the stage"); }, [] { Tick(); })) {
			return;  // another time
		}
		g_lastDay = today;
		g_played[{ her->GetFormID(), best }] = today;
		g_her = her->GetHandle();
		g_child = child->GetHandle();
		g_talk = &talk;
		g_number = best;
		g_step = -1;
		g_stepDay = today;
		const auto& f = Forms::Get();
		her->AddToFaction(f.family, static_cast<std::int8_t>(best));
		const bool childFirst = talk.steps.front().first;
		logger::info("Family: {} and {} talk (conversation {}, home {})", her->GetDisplayFullName(), child->GetDisplayFullName(), best, Rank(home));
		Script::CallStatic("IRF_Internal"sv, "Approach"sv, f.sayQuest, childFirst ? child : her, childFirst ? her : child, 2);
	}

	void OnArrived()
	{
		if (Running() && g_step < 0) {
			Say(0);
		}
	}

	void OnLineBegan(RE::Actor* a_speaker, const std::string& a_key)
	{
		if (Running() && a_speaker == Speaker(g_step) && a_key == std::format("family.{}.{}", g_number, g_step + 1)) {
			g_begun = true;
		}
	}

	void OnLineEnded(RE::Actor* a_speaker, const std::string& a_key)
	{
		if (!Running() || a_speaker != Speaker(g_step) || a_key != std::format("family.{}.{}", g_number, g_step + 1)) {
			return;
		}
		if (!g_begun) {
			// it ended without ever starting: cut off (a talk with you opened, or she was saying something else)
			if (RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
				g_waitTalk = true;  // again once the talk is over
			} else {
				g_stepDay = 0.0f;  // Tick says it again at the next line around
			}
			return;
		}
		const auto& f = Forms::Get();
		a_speaker->AddToFaction(f.familyStep, 0);
		if (a_speaker->GetFactionRank(f.moment, false) == Rank(Moment::kFamily)) {
			a_speaker->AddToFaction(f.moment, 0);
		}
		if (g_step + 1 < static_cast<int>(g_talk->steps.size())) {
			Say(g_step + 1);
		} else {
			End("done");
		}
	}

	void OnDialogueClosed()
	{
		if (Running() && std::exchange(g_waitTalk, false)) {
			if (const auto speaker = Speaker(g_step)) {
				g_begun = false;
				g_stepDay = State::Today();
				World::SayMoment(speaker, Moment::kFamily, 1.5f);  // a line within ~1 s of a talk closing never plays
			}
		}
	}

	void Tick()
	{
		if (!Running()) {
			return;
		}
		const auto her = g_her.get();
		const auto child = g_child.get();
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto reach = Rules::Get().familyHearDistance * 2.0f;
		if (!her || !child || her->IsDead() || child->IsDead() || her->IsInCombat() || child->IsInCombat() || player->IsInCombat() ||
			!World::SamePlace(her.get(), player) || !World::SamePlace(child.get(), player) || player->GetPosition().GetDistance(her->GetPosition()) > reach) {
			End("broken off");
			return;
		}
		const auto speaker = Speaker(g_step);
		if (g_waitTalk) {
			return;  // a talk with you is on: the line waits for it
		}
		if (speaker && !g_begun && RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
			g_waitTalk = true;  // said while you talk to someone, it never plays: again once the talk is over
			return;
		}
		if (speaker && !g_begun && State::Today() - g_stepDay > 0.002f) {  // ~3 game minutes: a line that didn't start
			if (g_retries >= 2) {
				End("a line never played");
				return;
			}
			++g_retries;
			g_stepDay = State::Today();
			World::SayMoment(speaker, Moment::kFamily, 0.3f);
			return;
		}
		if (State::Today() - g_stepDay > 0.03f) {
			End("it stalled");
		}
	}

	void OnGameLoaded()
	{
		// a conversation in a save isn't picked up again: whatever it left on them goes
		const auto& f = Forms::Get();
		for (const auto& candidate : Candidates::All()) {
			if (const auto actor = Candidates::ActorOf(candidate); actor && actor->GetFactionRank(f.family, false) > 0) {
				actor->AddToFaction(f.family, 0);
			}
		}
		for (const auto& [ref, data] : State::Children()) {
			if (const auto child = RE::TESForm::LookupByID<RE::Actor>(ref); child && child->GetFactionRank(f.familyStep, false) > 0) {
				child->AddToFaction(f.familyStep, 0);
			}
		}
		g_her = {};
		g_child = {};
		g_talk = nullptr;
		g_number = 0;
		g_step = -1;
		g_lastDay = -1.0f;
		g_waitTalk = false;
	}
}
