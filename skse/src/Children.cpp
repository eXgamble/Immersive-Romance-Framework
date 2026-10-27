#include "Children.h"

#include "Candidates.h"
#include "Follower.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "Scenes.h"
#include "Script.h"
#include "State.h"

namespace Children
{
	using namespace IRF;

	namespace
	{
		// game thread (SKSE tasks)
		RE::ActorHandle g_stayHome;            // the spouse on her way to tell you she stays home
		float           g_stayHomeDay{ 0.0f };
		bool            g_leftTown{ false };   // left the family's town during a loading screen: decided when it closes
		bool            g_fastTravel{ false };

		// The married candidate (none: no lines, no staying home)
		RE::Actor* Spouse()
		{
			const auto partner = Romance::Partner();
			return partner && Romance::GetStage(partner) == Stage::kMarried ? partner : nullptr;
		}

		// Hearthfire's adopted children (its Child1 and Child2 aliases)
		std::vector<RE::Actor*> Adopted()
		{
			std::vector<RE::Actor*> out;
			const auto              quest = Forms::Get().adoption;
			if (!quest) {
				return out;
			}
			for (const auto alias : quest->aliases) {
				if (alias && alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID && (alias->aliasName == "Child1"sv || alias->aliasName == "Child2"sv)) {
					if (const auto child = static_cast<RE::BGSRefAlias*>(alias)->GetActorReference(); child && !child->IsDead()) {
						out.push_back(child);
					}
				}
			}
			return out;
		}

		// The family's town: where the first child lives (Hearthfire moves the family together); a homestead outside
		// any town counts as its own place
		RE::BGSLocation* HomeTown()
		{
			const auto children = Adopted();
			const auto loc = children.empty() ? nullptr : children.front()->GetCurrentLocation();
			const auto town = loc ? Candidates::TownOf(loc) : nullptr;
			return town ? town : loc;
		}

		bool InHomeTown(RE::BGSLocation* a_location)
		{
			const auto home = HomeTown();
			if (!home) {
				return true;  // can't tell: never keep her back
			}
			for (auto loc = a_location; loc; loc = loc->parentLoc) {
				if (loc == home) {
					return true;
				}
			}
			return false;
		}

		ChildMood MoodOf(RE::Actor* a_child)
		{
			const auto spouse = Spouse();
			if (!spouse) {
				return ChildMood::kNone;
			}
			const auto today = State::Today();
			auto&      c = State::Children()[a_child->GetFormID()];
			if (c.lastSeenDay <= State::kNever) {
				c.lastSeenDay = today;  // first met since adoption
			}
			const float away = today - c.lastSeenDay;
			const auto  days = static_cast<float>(std::max(1, Rules::Get().childAwayDays));
			if (away >= 3.0f * days) {
				return ChildMood::kAway3;
			}
			if (away >= 2.0f * days) {
				return ChildMood::kAway2;
			}
			if (away >= days) {
				return ChildMood::kAway1;
			}
			if (today - c.lastMoodDay < 1.0f) {
				return ChildMood::kNone;  // said it today already
			}
			const auto& s = State::Of(spouse->GetFormID());
			if (s.hurt || s.silentUntilDay > today) {
				return ChildMood::kHeartbroken;
			}
			const int band = 1 + std::min(4, Romance::GetApproval(spouse) / 20);  // as "How are we?"
			return band == 1 ? ChildMood::kBad : band == 2 ? ChildMood::kStrained : ChildMood::kNone;
		}

		void SetMood(RE::Actor* a_child, ChildMood a_mood)
		{
			const auto faction = Forms::Get().child;
			const int  rank = Rank(a_mood);
			const int  now = a_child->IsInFaction(faction) ? a_child->GetFactionRank(faction, false) : 0;
			if (now != rank) {
				a_child->AddToFaction(faction, static_cast<std::int8_t>(rank));
			}
		}

		void StayHomeOff()
		{
			if (const auto spouse = g_stayHome.get()) {
				Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, spouse.get(), false);
			}
			g_stayHome = {};
			Scenes::End(Scenes::Kind::kStayHome);
		}

		// Walked to the edge: she stops following, comes up to you and says why, then goes home
		void ForceGreet(RE::Actor* a_spouse)
		{
			if (!Scenes::Begin(Scenes::Kind::kStayHome, { a_spouse }, {})) {
				return;  // the scene on can't be broken off: at the next place
			}
			Follower::Dismiss(a_spouse);
			a_spouse->AddToFaction(Forms::Get().moment, static_cast<std::int8_t>(Rank(Moment::kStayHome)));
			Script::CallStatic("IRF_Internal"sv, "ForceGreet"sv, Forms::Get().sayQuest, a_spouse, true);
			g_stayHome = a_spouse->GetHandle();
			g_stayHomeDay = State::Today();
			logger::info("Children: {} won't leave the children, she comes to tell you", a_spouse->GetDisplayFullName());
		}

		// A fast travel took you away: she didn't come; a message says it
		void StayBehind(RE::Actor* a_spouse)
		{
			StayHomeOff();
			Follower::Dismiss(a_spouse);
			Script::CallStatic("IRF_Internal"sv, "SendHome"sv, a_spouse);
			RE::SendHUDMessage::ShowHUDMessage(std::format("{} stays home with the children.", a_spouse->GetDisplayFullName()).c_str());
			logger::info("Children: {} stays home with the children (fast travel)", a_spouse->GetDisplayFullName());
		}

		// The spouse is with you, out of the family's town, while you have children
		RE::Actor* SpouseLeavingTown(RE::BGSLocation* a_location)
		{
			const auto spouse = Spouse();
			if (!spouse || !spouse->IsPlayerTeammate() || Adopted().empty() || InHomeTown(a_location)) {  // no location: the wilds
				return nullptr;
			}
			return spouse;
		}
	}

	int Boys()
	{
		return static_cast<int>(std::ranges::count_if(Adopted(), [](RE::Actor* a) { return a->GetActorBase() && a->GetActorBase()->GetSex() == RE::SEX::kMale; }));
	}

	int Girls()
	{
		return static_cast<int>(std::ranges::count_if(Adopted(), [](RE::Actor* a) { return a->GetActorBase() && a->GetActorBase()->GetSex() == RE::SEX::kFemale; }));
	}

	void Count()
	{
		const auto& f = Forms::Get();
		const auto  boys = static_cast<float>(Boys());
		const auto  girls = static_cast<float>(Girls());
		if (f.adoptedBoys && f.adoptedGirls && (f.adoptedBoys->value != boys || f.adoptedGirls->value != girls)) {
			// -1: never counted in this game (the plugin's value; added mid-game): the children already there are
			// no news to anyone
			const bool counted = f.adoptedBoys->value >= 0.0f && f.adoptedGirls->value >= 0.0f;
			const bool more = boys + girls > f.adoptedBoys->value + f.adoptedGirls->value;
			f.adoptedBoys->value = boys;
			f.adoptedGirls->value = girls;
			logger::info("Children: {} boys, {} girls", boys, girls);
			const auto spouse = Romance::Partner();
			if (counted && more && spouse && Romance::GetStage(spouse) == Stage::kMarried) {
				Romance::Notify(spouse, Event::kAdopted);  // her family hears of it
			}
		}
	}

	bool IsChild(RE::Actor* a_actor)
	{
		const auto faction = Forms::Get().adoptionFaction;
		return a_actor && faction && a_actor->IsInFaction(faction);
	}

	void Sync(RE::Actor* a_child)
	{
		if (IsChild(a_child)) {
			SetMood(a_child, MoodOf(a_child));
		}
	}

	void OnGreeted(RE::Actor* a_child)
	{
		auto&      c = State::Children()[a_child->GetFormID()];
		const auto today = State::Today();
		const int  rank = a_child->GetFactionRank(Forms::Get().child, false);
		c.lastSeenDay = today;
		c.lastMoodDay = today;  // one "something's wrong" a day, whichever it was
		logger::info("Children: {} greeted you ({})", a_child->GetDisplayFullName(), rank);
		Sync(a_child);
	}

	void OnTalked(RE::Actor* a_child)
	{
		State::Children()[a_child->GetFormID()].lastSeenDay = State::Today();
		Sync(a_child);
	}

	void OnPlayerLocation(RE::BGSLocation* a_location)
	{
		if (g_stayHome && State::Today() - g_stayHomeDay > 0.05f) {
			StayHomeOff();  // she never reached you (an hour of game time): let her go home
		}
		const auto spouse = SpouseLeavingTown(a_location);
		if (!spouse || g_stayHome) {
			return;
		}
		if (RE::UI::GetSingleton()->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
			g_leftTown = true;  // a door or a fast travel: told apart when the loading screen closes
			return;
		}
		ForceGreet(spouse);
	}

	void OnLoadingDone()
	{
		if (!std::exchange(g_leftTown, false)) {
			g_fastTravel = false;
			return;
		}
		const auto spouse = SpouseLeavingTown(RE::PlayerCharacter::GetSingleton()->GetCurrentLocation());
		if (spouse) {
			if (std::exchange(g_fastTravel, false)) {
				StayBehind(spouse);
			} else {
				ForceGreet(spouse);
			}
		}
		g_fastTravel = false;
	}

	void OnFastTravelEnd()
	{
		g_fastTravel = true;
		// the event may come after the loading screen closed: she may already be on her way to tell you
		const auto spouse = g_stayHome.get();
		if (spouse && !InHomeTown(RE::PlayerCharacter::GetSingleton()->GetCurrentLocation())) {
			StayBehind(spouse.get());
			g_fastTravel = false;
		} else if (const auto s = SpouseLeavingTown(RE::PlayerCharacter::GetSingleton()->GetCurrentLocation()); s && !g_leftTown) {
			StayBehind(s);
			g_fastTravel = false;
		}
	}

	void OnStayHomeSaid(RE::Actor* a_spouse)
	{
		if (g_stayHome.get().get() == a_spouse) {
			StayHomeOff();
			logger::info("Children: {} goes home", a_spouse->GetDisplayFullName());
		}
	}

	void OnGameLoaded()
	{
		g_stayHome = {};
		g_leftTown = false;
		g_fastTravel = false;
	}
}
