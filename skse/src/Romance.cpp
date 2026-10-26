#include "Romance.h"

#include "Expose.h"

#include "FirstDate.h"
#include "Follower.h"
#include "Forms.h"
#include "Rules.h"
#include "Script.h"
#include "State.h"
#include "World.h"

namespace Romance
{
	using namespace IRF;

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

		// Running the way the game means it (Papyrus IsRunning / GetQuestRunning): enabled and not stopping.
		// CommonLib's TESQuest::IsRunning() is also true for a stopped quest (measured 2026-10-25).
		bool Running(const RE::TESQuest* a_quest)
		{
			return a_quest && a_quest->IsEnabled() && !a_quest->IsStopping();
		}

		bool StageReached(const RE::TESQuest* a_quest, std::uint16_t a_stage)
		{
			return a_quest && (a_quest->IsCompleted() || a_quest->GetCurrentStageID() >= a_stage);
		}

		// The quests whose aliases hold the actor right now
		std::vector<RE::TESQuest*> AliasQuests(RE::Actor* a_actor)
		{
			std::vector<RE::TESQuest*> quests;
			const auto                 extra = a_actor->extraList.GetByType<RE::ExtraAliasInstanceArray>();
			if (!extra) {
				return quests;
			}
			RE::BSReadLockGuard locker{ extra->lock };
			for (const auto data : extra->aliases) {
				if (data && data->quest) {
					quests.push_back(data->quest);
				}
			}
			return quests;
		}

		bool InAliasOf(RE::Actor* a_actor, const RE::TESQuest* a_quest)
		{
			const auto quests = AliasQuests(a_actor);
			return a_quest && std::ranges::find(quests, a_quest) != quests.end();
		}

		// A running story quest (not generic dialogue, not ours, not the marriage or follower system) holds them
		bool QuestBusy(RE::Actor* a_actor)
		{
			const auto& f = Forms::Get();
			for (const auto quest : AliasQuests(a_actor)) {
				if (!Running(quest) || quest->GetType() == RE::QUEST_DATA::Type::kNone) {
					continue;
				}
				if (std::ranges::find(f.quests, quest) != f.quests.end() || quest == f.followerQuest || quest == f.marriage || quest == f.wedding) {
					continue;
				}
				return true;
			}
			return false;
		}

		// When vanilla would offer its amulet line: the player wears the Amulet of Mara, Maramal has told them about
		// marriage (RelationshipMarriage at stage 10 or later), they aren't married and no wedding is running
		bool AmuletOffered()
		{
			const auto& f = Forms::Get();
			const auto  player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->GetWornArmor(f.amuletOfMara->GetFormID())) {
				return false;
			}
			if (player->IsInFaction(f.playerMarried) || Running(f.wedding)) {
				return false;
			}
			return Running(f.marriage) && f.marriage->GetCurrentStageID() >= 10;
		}

		bool GateOpen(const Candidates::Candidate& a_candidate, Stage a_stage)
		{
			const auto it = a_candidate.attributes.gates.find(a_stage);
			return it == a_candidate.attributes.gates.end() || StageReached(it->second.quest, it->second.stage);
		}

		void SendModEvent(std::string_view a_name, RE::TESForm* a_sender, std::string_view a_str, float a_num)
		{
			SKSE::ModCallbackEvent event{ RE::BSFixedString(a_name), RE::BSFixedString(a_str), a_num, a_sender };
			SKSE::GetModCallbackEventSource()->SendEvent(&event);
		}

		Event EventFor(Stage a_stage)
		{
			switch (a_stage) {
			case Stage::kDating:
				return Event::kDating;
			case Stage::kCommitted:
				return Event::kCommitted;
			case Stage::kEngaged:
				return Event::kEngaged;
			case Stage::kMarried:
				return Event::kMarried;
			default:
				return Event::kNone;
			}
		}

		GreetState GreetFor(Stage a_stage, const State::Candidate& a_state, bool a_hint)
		{
			if (a_state.silentUntilDay > State::Today()) {
				return GreetState::kBrokenSilent;
			}
			if (a_state.hurt) {
				return GreetState::kBrokenHurt;
			}
			switch (a_stage) {
			case Stage::kFriend:
				return a_hint ? GreetState::kHint : GreetState::kNone;  // else vanilla's Friend greetings
			case Stage::kDating:
				return GreetState::kDating;
			case Stage::kCommitted:
				return GreetState::kCommitted;
			case Stage::kEngaged:
				return GreetState::kEngaged;
			case Stage::kMarried:
				return GreetState::kSpouse;
			case Stage::kEx:
				if (a_state.pleadsLeft > 0 && a_state.nextPleadDay <= State::Today()) {
					return GreetState::kPlead;  // she asks you to take her back
				}
				return static_cast<GreetState>(IRF::Rank(GreetState::kEx) + IRF::Rank(a_state.why == Why::kNone ? Why::kChoice : a_state.why));
			default:
				return GreetState::kNone;
			}
		}

		// Approval passed a line: the stage follows (commitment bar), or they leave you when it runs out
		void CheckThresholds(RE::Actor* a_actor, Why a_why)
		{
			const auto  candidate = Candidates::Find(a_actor);
			const auto  stage = GetStage(a_actor);
			const auto  approval = GetApproval(a_actor);
			const auto& r = Rules::Get();
			if (stage == Stage::kDating && approval >= r.commitBar && GateOpen(*candidate, Stage::kCommitted)) {
				SetStage(a_actor, Stage::kCommitted, "approval passed the commitment bar");
			} else if (stage == Stage::kCommitted && approval < r.commitBar) {
				SetStage(a_actor, Stage::kDating, "approval fell below the commitment bar");
			}
			if (stage >= Stage::kDating && stage <= Stage::kEngaged && approval < r.floorDating) {
				auto& s = State::Of(a_actor->GetFormID());
				if (s.pendingBreakup == Why::kNone) {
					s.pendingBreakup = a_why == Why::kNone ? Why::kAbsence : a_why;
					logger::info("Romance: {} has had enough (why {}); she ends it at the next talk", a_actor->GetDisplayFullName(), IRF::Rank(s.pendingBreakup));
					Follower::Dismiss(a_actor);
				}
			}
		}

		void CallOffEngagement()
		{
			const auto& f = Forms::Get();
			if (Running(f.wedding)) {
				Script::Call(f.wedding, "RelationshipMarriageWeddingScript"sv, "UnregisterForUpdateGameTime"sv);
				f.wedding->Stop();
			}
			f.marriage->Stop();
			Script::Call(f.marriage, "Quest"sv, "SetStage"sv, static_cast<std::int32_t>(10));
			logger::info("Romance: engagement called off (wedding stopped, marriage quest back at 10)");
		}
	}

	bool IsPlayerInterested(const Candidates::Candidate& a_candidate)
	{
		const auto player = RE::PlayerCharacter::GetSingleton()->GetActorBase();
		if (!player || !a_candidate.npc) {
			return false;
		}
		const bool playerMale = player->GetSex() == RE::SEX::kMale;
		const bool theyMale = a_candidate.npc->GetSex() == RE::SEX::kMale;
		const auto interest = State::PlayerInterest().empty() ? Rules::PlayerInterest() : State::PlayerInterest();
		if ((interest == "male" && !theyMale) || (interest == "female" && theyMale)) {
			return false;
		}
		using I = Candidates::Interest;
		const auto want = a_candidate.attributes.interestedIn;
		return want == I::kAny || (want == I::kMale) == playerMale;
	}

	Stage GetStage(RE::Actor* a_actor)
	{
		return static_cast<Stage>(Rank(a_actor, Forms::Get().stage));
	}

	int GetApproval(RE::Actor* a_actor)
	{
		return std::max(0, Rank(a_actor, Forms::Get().approval));
	}

	std::vector<RE::Actor*> Partners()
	{
		std::vector<RE::Actor*> out;
		for (const auto& candidate : Candidates::All()) {
			const auto actor = Candidates::ActorOf(candidate);
			if (IsPartner(actor)) {
				out.push_back(actor);
			}
		}
		return out;
	}

	bool IsPartner(RE::Actor* a_actor)
	{
		if (!a_actor || a_actor->IsDead() || !Candidates::Find(a_actor)) {
			return false;
		}
		const auto stage = GetStage(a_actor);
		return stage >= Stage::kDating && stage <= Stage::kMarried;
	}

	bool Knows(RE::Actor* a_who, RE::Actor* a_partner)
	{
		return a_who && a_partner && State::Of(a_partner->GetFormID()).knowSeeing.contains(a_who->GetFormID());
	}

	RE::Actor* Partner()
	{
		RE::Actor* best = nullptr;
		auto       bestStage = Stage::kNone;
		for (const auto& candidate : Candidates::All()) {
			const auto actor = Candidates::ActorOf(candidate);
			const auto stage = actor ? GetStage(actor) : Stage::kNone;
			if (stage >= Stage::kDating && stage <= Stage::kMarried && stage > bestStage && !actor->IsDead()) {
				best = actor;
				bestStage = stage;
			}
		}
		return best;
	}

	void SetStage(RE::Actor* a_actor, Stage a_stage, std::string_view a_cause)
	{
		if (a_stage == Stage::kCourting) {
			a_stage = Stage::kFriend;  // retired: a Friend is the courting phase
		}
		const auto candidate = Candidates::Find(a_actor);
		if (!candidate) {
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
			SetRank(a_actor, f.stage, IRF::Rank(a_stage));
		}
		logger::info("Romance: {} {} -> {} ({})", a_actor->GetDisplayFullName(), IRF::Rank(old), IRF::Rank(a_stage), a_cause);

		auto& s = State::Of(a_actor->GetFormID());
		if (a_stage == Stage::kCommitted && old < Stage::kCommitted) {
			s.committedDay = State::Today();  // she tells you how much you mean to her a little later (Confess)
		} else if (a_stage > Stage::kCommitted && a_stage <= Stage::kMarried) {
			s.confessed = true;  // past that moment
		} else if (a_stage == Stage::kEx) {
			s.confessed = false;  // a new relationship (a win-back) gets its own
		}
		if (a_stage == Stage::kEngaged || a_stage == Stage::kMarried) {
			s.secret = false;  // an engagement or a wedding ends a secret: everyone knows
			for (const auto& other : Candidates::All()) {
				if (other.ref == candidate->ref || !s.knowSeeing.insert(other.ref).second) {
					continue;
				}
				// another partner hears of it: word got back, and there's no denying an engagement
				const auto actor = RE::TESForm::LookupByID<RE::Actor>(other.ref);
				if (IsPartner(actor)) {
					auto& os = State::Of(other.ref);
					os.confrontPending = true;
					os.confrontWorks = false;
					logger::info("Romance: {} will hear you're {} to {}", actor->GetDisplayFullName(),
						a_stage == Stage::kEngaged ? "engaged" : "married", a_actor->GetDisplayFullName());
				}
			}
		}
		if (const auto event = EventFor(a_stage); event != Event::kNone && a_stage > old) {
			Notify(a_actor, event);
		}
		if (a_stage < Stage::kDating || a_stage > Stage::kMarried) {
			Follower::Dismiss(a_actor);
		}
		Sync(a_actor);
		SendModEvent("IRF_StageChanged"sv, a_actor, a_cause, static_cast<float>(a_stage));
	}

	void ModApproval(RE::Actor* a_actor, int a_amount, std::string_view a_cause, Why a_why)
	{
		if (!Candidates::Find(a_actor) || a_amount == 0 || GetStage(a_actor) == Stage::kNone) {
			return;
		}
		const auto before = GetApproval(a_actor);
		const auto after = std::clamp(before + a_amount, 0, 100);
		SetRank(a_actor, Forms::Get().approval, after);
		logger::info("Romance: {} approval {} -> {} ({:+}, {})", a_actor->GetDisplayFullName(), before, after, a_amount, a_cause);
		CheckThresholds(a_actor, a_why);
		Sync(a_actor);
	}

	void Hurt(RE::Actor* a_actor, const Candidates::Tolerance& a_tolerance, Why a_why, std::string_view a_cause, int a_times)
	{
		const auto stage = GetStage(a_actor);
		if (a_tolerance.never && stage >= Stage::kDating && stage <= Stage::kEngaged) {
			auto& s = State::Of(a_actor->GetFormID());
			s.unforgivable = true;
			s.pendingBreakup = a_why;
			SetRank(a_actor, Forms::Get().approval, 0);
			logger::info("Romance: {} won't forgive this ({}); she ends it", a_actor->GetDisplayFullName(), a_cause);
			Follower::Dismiss(a_actor);
			Sync(a_actor);
			return;
		}
		// "never" on a spouse counts as the lowest tolerance: the biggest drop, the marriage stands
		const int cost = a_tolerance.never ? 100 : a_tolerance.cost * a_times;
		ModApproval(a_actor, -cost, a_cause, a_why);
	}

	void BreakUp(RE::Actor* a_actor, Who a_who, Why a_why, bool a_unforgivable)
	{
		const auto stage = GetStage(a_actor);
		if (stage < Stage::kDating || stage > Stage::kEngaged) {
			return;
		}
		auto& s = State::Of(a_actor->GetFormID());
		if (stage == Stage::kEngaged) {
			CallOffEngagement();
		}
		s.who = a_who;
		s.why = a_why;
		s.breakupDay = State::Today();
		s.unforgivable = s.unforgivable || a_unforgivable;
		s.pendingBreakup = Why::kNone;
		s.hurt = false;
		s.silentUntilDay = 0.0f;
		s.confrontPending = false;
		s.pleadsLeft = 0;
		// how she takes it when you end it (her heartbreak style)
		if (a_who == Who::kPlayer && a_why == Why::kChoice && !s.unforgivable) {
			const auto candidate = Candidates::Find(a_actor);
			if (candidate && candidate->attributes.heartbreak == Candidates::Heartbreak::kBitter) {
				s.silentUntilDay = s.breakupDay + static_cast<float>(candidate->attributes.silentDays);  // her brush-off
			} else if (candidate && candidate->attributes.heartbreak == Candidates::Heartbreak::kPleading) {
				s.pleadsLeft = candidate->attributes.pleads;
				s.nextPleadDay = s.breakupDay;  // the next time you talk to her
			}
		}
		const auto& f = Forms::Get();
		SetRank(a_actor, f.who, IRF::Rank(a_who));
		SetRank(a_actor, f.why, IRF::Rank(a_why));
		SetStage(a_actor, Stage::kEx, a_who == Who::kPlayer ? "the player ended it" : "they ended it");
		Notify(a_actor, a_who == Who::kPlayer ? Event::kPlayerEnded : Event::kTheyEnded);
	}

	void Caught(RE::Actor* a_partner, std::string_view a_cause, float a_delay, RE::Actor* a_with)
	{
		const auto candidate = Candidates::Find(a_partner);
		if (!candidate) {
			return;
		}
		logger::info("Romance: {} caught you ({})", a_partner->GetDisplayFullName(), a_cause);
		if (a_with && Candidates::Find(a_with)) {
			State::Of(a_with->GetFormID()).knowSeeing.insert(a_partner->GetFormID());  // she knows about her now
		}
		// in a talk her answer is the line (deny.fails, admit); a moment line said then would never play
		if (!RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
			World::SayMoment(a_partner, Moment::kCaught, a_delay);
		}
		Heartbroken(a_partner, a_cause);
		SendModEvent("IRF_Strayed"sv, a_partner, a_cause, 1.0f);
	}

	void Heartbroken(RE::Actor* a_partner, std::string_view a_cause, int a_times)
	{
		const auto candidate = Candidates::Find(a_partner);
		if (!candidate) {
			return;
		}
		auto& s = State::Of(a_partner->GetFormID());
		int   silent = candidate->attributes.brokenHeartDays;
		if (candidate->attributes.heartbreak == Candidates::Heartbreak::kBitter) {
			silent = std::max(silent, candidate->attributes.silentDays);  // a bitter heart stays silent longer
		}
		s.silentUntilDay = State::Today() + static_cast<float>(silent * std::max(1, a_times));
		s.hurt = true;
		Follower::Dismiss(a_partner);
		Notify(a_partner, Event::kBrokenHeart);
		Hurt(a_partner, candidate->attributes.cheat, Why::kCheating, a_cause);
		Sync(a_partner);
	}

	bool WontBeCourted(RE::Actor* a_actor)
	{
		const auto candidate = Candidates::Find(a_actor);
		if (!candidate || IsPartner(a_actor) || candidate->attributes.scruples == Candidates::Scruples::kCompete) {
			return false;
		}
		for (const auto partner : Partners()) {
			const auto& ps = State::Of(partner->GetFormID());
			if (ps.knowSeeing.contains(candidate->ref) || GetStage(partner) >= Stage::kEngaged) {
				return true;  // she knows you're taken
			}
		}
		return false;
	}

	void Strayed(RE::Actor* a_other, std::string_view a_deed)
	{
		const auto& r = Rules::Get();
		const auto  player = RE::PlayerCharacter::GetSingleton();
		float       delay = 0.0f;
		bool        seen = false;
		for (const auto partner : Partners()) {
			if (partner == a_other) {
				continue;
			}
			if (World::Sees(partner, player, r.caughtDistance) && IsPartner(a_other) && !Knows(partner, a_other) && Expose::Start(a_other, partner)) {
				return;  // your other partner saw it: the two have it out
			}
			if (World::Sees(partner, player, r.caughtDistance)) {
				Caught(partner, a_deed, delay, a_other);
				delay += r.caughtLineGap;
				seen = true;
				continue;
			}
			// not caught: who saw it may tell her later (people of her community: candidates and her circle)
			const auto pc = Candidates::Find(partner);
			auto&      s = State::Of(partner->GetFormID());
			for (const auto witness : World::Witnesses(player, r.witnessDistance)) {
				if (witness == partner || witness == a_other) {
					continue;
				}
				const auto wc = Candidates::Find(witness);
				bool       ofHerCircle = false;
				for (const auto& [c, member] : Candidates::CircleOf(witness->GetFormID())) {
					ofHerCircle = ofHerCircle || c == pc;
				}
				if ((wc && wc->community == pc->community) || ofHerCircle) {
					s.sawStraying.insert(witness->GetFormID());
					logger::info("Romance: {} saw you stray from {}", witness->GetDisplayFullName(), partner->GetDisplayFullName());
				}
			}
			SendModEvent("IRF_Strayed"sv, partner, a_deed, 0.0f);
		}
		// your other partner was right there, so it comes out for her too: both catch you on the spot (user, 2026-10-26)
		if (seen && IsPartner(a_other)) {
			for (const auto partner : Partners()) {
				if (partner != a_other && Knows(partner, a_other)) {
					Caught(a_other, "caught by your other partner", delay, partner);
					break;
				}
			}
		}
	}

	void Notify(RE::Actor* a_candidate, Event a_event)
	{
		const auto candidate = Candidates::Find(a_candidate);
		if (!candidate) {
			return;
		}
		const auto  today = State::Today();
		const float until = today + static_cast<float>(Rules::Get().reactionDays);
		auto&       s = State::Of(a_candidate->GetFormID());
		s.recent = a_event;
		s.recentDay = today;

		auto pend = [&](RE::FormID a_ref, Event a_what) {
			State::Reactions()[a_ref] = { a_what, until };
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(a_ref)) {
				Sync(actor);
			}
		};
		if (candidate->reactsTo.contains(a_event) || a_event == Event::kStoodUp) {  // her stood-up line is required
			pend(candidate->ref, a_event);
		}
		const auto eventName = Candidates::EventName(a_event);
		int        index = 0;
		for (const auto& member : candidate->circle) {
			const auto key = std::format("circle.{}.reaction.{}", index, eventName);
			if (candidate->lines.contains(key)) {
				pend(member.ref, a_event);
			}
			if (member.refuseOn.contains(a_event) || (a_event == Event::kTheyEnded && member.refuseOnBreakup.contains(s.why))) {
				State::Refusals()[member.ref] = { candidate->ref, member.refuseDays > 0 ? today + static_cast<float>(member.refuseDays) : 0.0f };
				logger::info("Romance: {} refuses you service over {}", member.name, candidate->name);
				if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(member.ref)) {
					SyncOther(actor);
				}
			}
			++index;
		}
		// you're seeing her: the other candidates who know react
		if (a_event == Event::kDating || a_event == Event::kEngaged || a_event == Event::kMarried) {
			for (const auto& other : Candidates::All()) {
				if (other.ref != candidate->ref && other.reactsTo.contains(Event::kSeeingSomeone) && s.knowSeeing.contains(other.ref)) {
					pend(other.ref, Event::kSeeingSomeone);
				}
			}
		}
		// amends and a win-back end refusals that wait for amends
		if (a_event == Event::kAmends || a_event == Event::kWonBack) {
			std::erase_if(State::Refusals(), [&](const auto& a_pair) { return a_pair.second.because == candidate->ref && a_pair.second.untilDay == 0.0f; });
			for (const auto& member : candidate->circle) {
				if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(member.ref)) {
					SyncOther(actor);
				}
			}
		}
	}

	void Contact(RE::Actor* a_actor)
	{
		if (Candidates::Find(a_actor)) {
			State::Of(a_actor->GetFormID()).lastContactDay = State::Today();
		}
	}

	void SyncOther(RE::Actor* a_actor)
	{
		const auto& f = Forms::Get();
		const auto  today = State::Today();
		const auto  id = a_actor->GetFormID();

		if (!Candidates::Find(a_actor)) {
			int reaction = 0;
			if (const auto it = State::Reactions().find(id); it != State::Reactions().end()) {
				if (it->second.untilDay < today) {
					State::Reactions().erase(it);
				} else {
					reaction = IRF::Rank(it->second.event);
				}
			}
			if (reaction > 0 || InFaction(a_actor, f.reaction)) {
				SetRank(a_actor, f.reaction, reaction);
			}
		}

		int refuse = 0;
		if (const auto it = State::Refusals().find(id); it != State::Refusals().end()) {
			if (it->second.untilDay > 0.0f && it->second.untilDay < today) {
				State::Refusals().erase(it);
			} else {
				refuse = 1;
			}
		}
		if (refuse > 0 || InFaction(a_actor, f.refuse)) {
			SetRank(a_actor, f.refuse, refuse);
		}
	}

	namespace
	{
		// Her own spouse perk (her add-on's spouseBonus): on while you're married and the marriage is in good standing,
		// paused while she's hurt or approval is below spouseBonusBelow, gone if she dies (Widowed). The framework owns
		// when; the add-on owns what. IRF_SpouseBonusFaction (1 = on) lets lines read it, and the IRF_SpouseBonus mod
		// event ("on" / "off") tells add-on scripts
		void SyncSpouseBonus(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, Stage a_stage, const State::Candidate& a_state)
		{
			const auto& f = Forms::Get();
			const bool  wanted = a_stage == Stage::kMarried && !a_actor->IsDead() && !a_state.hurt && a_state.silentUntilDay <= State::Today() &&
			                    GetApproval(a_actor) >= Rules::Get().spouseBonusBelow;
			const bool  now = InFaction(a_actor, f.spouseBonus) && Rank(a_actor, f.spouseBonus) > 0;
			if (wanted == now) {
				return;
			}
			SetRank(a_actor, f.spouseBonus, wanted ? 1 : 0);
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (const auto spell = a_candidate.attributes.spouseBonus ? a_candidate.attributes.spouseBonus->As<RE::SpellItem>() : nullptr) {
				wanted ? player->AddSpell(spell) : player->RemoveSpell(spell);
			} else if (const auto perk = a_candidate.attributes.spouseBonus ? a_candidate.attributes.spouseBonus->As<RE::BGSPerk>() : nullptr) {
				wanted ? player->AddPerk(perk) : player->RemovePerk(perk);
			}
			logger::info("Romance: {}'s spouse perk {}", a_actor->GetDisplayFullName(), wanted ? "on" : "off");
			SendModEvent("IRF_SpouseBonus"sv, a_actor, wanted ? "on"sv : "off"sv, wanted ? 1.0f : 0.0f);
		}
	}

	void SyncChat(RE::Actor* a_actor)
	{
		const auto& f = Forms::Get();
		if (!Candidates::Find(a_actor)) {
			return;
		}
		const auto& s = State::Of(a_actor->GetFormID());
		const bool  waiting = Rank(a_actor, f.reaction) > 0 || Rank(a_actor, f.plead) > 0 ||
		                     Rank(a_actor, f.moment) == IRF::Rank(Moment::kConfess) || s.pendingBreakup != Why::kNone || s.confrontPending;
		const bool  chat = a_actor->IsPlayerTeammate() && !a_actor->IsDead() && !s.hurt && s.silentUntilDay <= State::Today() && !waiting;
		SetRank(a_actor, f.chat, chat ? 1 : 0);
	}

	void Sync(RE::Actor* a_actor, bool a_talking)
	{
		const auto candidate = Candidates::Find(a_actor);
		if (!candidate) {
			SyncOther(a_actor);
			return;
		}
		const auto& f = Forms::Get();
		const auto& r = Rules::Get();
		const auto  today = State::Today();
		auto&       s = State::Of(a_actor->GetFormID());
		SetRank(a_actor, f.candidate, 0);
		SyncOther(a_actor);

		// Friend: vanilla's rank 1 (favor quests), and someone they and the player would court
		auto       stage = GetStage(a_actor);
		const bool interested = IsPlayerInterested(*candidate);
		if (stage == Stage::kNone && interested && RelationshipRank(a_actor) >= 1) {
			SetRank(a_actor, f.stage, IRF::Rank(Stage::kFriend));
			logger::info("Romance: {} is now a Friend", a_actor->GetDisplayFullName());
			stage = Stage::kFriend;
		} else if (stage == Stage::kFriend && !interested) {
			a_actor->RemoveFromFaction(f.stage);
			stage = Stage::kNone;
		}
		if (stage != Stage::kNone && !InFaction(a_actor, f.approval)) {
			SetRank(a_actor, f.approval, r.startApproval);
		}

		// vanilla's engagement and wedding drive Engaged and Married; death makes the player a widow(er)
		if (stage >= Stage::kCommitted && stage < Stage::kMarried && a_actor->IsInFaction(f.playerMarried)) {
			SetStage(a_actor, Stage::kMarried, "the wedding");
			return;
		}
		if (stage == Stage::kCommitted && Running(f.marriage) && f.marriage->GetCurrentStageID() >= 20 && InAliasOf(a_actor, f.marriage)) {
			SetStage(a_actor, Stage::kEngaged, "accepted the Amulet of Mara");
			return;
		}
		if (stage == Stage::kEngaged && !Running(f.marriage) && !a_actor->IsInFaction(f.playerMarried)) {
			SetStage(a_actor, Stage::kCommitted, "the engagement ended");
			return;
		}
		if (a_actor->IsDead() && stage >= Stage::kDating && stage <= Stage::kMarried) {
			if (s.who == Who::kPlayer && s.why == Why::kViolence) {
				SetStage(a_actor, Stage::kKilled, "you killed them");  // seen: no condolences for your own victim
				Notify(a_actor, Event::kKilled);
				return;
			}
			SetStage(a_actor, Stage::kWidowed, "died");
			Notify(a_actor, Event::kWidowed);
			return;
		}

		const bool silent = s.silentUntilDay > today;
		const auto approval = GetApproval(a_actor);
		const bool following = a_actor->IsPlayerTeammate();
		if (following) {
			s.lastContactDay = today;
		}

		if (a_talking && !silent) {
			// absence: worked out once, at the first talk after a long time apart
			if (stage >= Stage::kDating && stage <= Stage::kMarried && s.lastContactDay > State::kNever && s.pendingBreakup == Why::kNone) {
				const auto gap = today - s.lastContactDay;
				const auto days = static_cast<float>(std::max(1, candidate->attributes.absenceDays));
				if (gap > days) {
					const int band = gap > 4 * days ? 3 : gap > 2 * days ? 2 : 1;
					SetRank(a_actor, f.absence, band);
					logger::info("Romance: {} missed you ({:.1f} days apart, band {})", a_actor->GetDisplayFullName(), gap, band);
					Hurt(a_actor, candidate->attributes.absence, Why::kAbsence, "absence", band);
				}
			}
			s.lastContactDay = today;
			// word gets back: someone who saw you stray tells her (their chance is how garrulous they are)
			if (!s.sawStraying.empty() && !s.confrontPending && stage >= Stage::kDating) {
				for (const auto witness : s.sawStraying) {
					const auto wc = Candidates::FindByRef(witness);
					const int  chance = (wc ? wc->attributes.garrulous : 30) * r.garrulousScale / 100;
					if (World::Roll(chance)) {
						s.confrontPending = true;
						const auto speech = RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeech);
						s.confrontWorks = speech >= (f.speechVeryHard ? f.speechVeryHard->value : 100.0f);
						logger::info("Romance: word got back to {} (a denial will {})", a_actor->GetDisplayFullName(), s.confrontWorks ? "work" : "fail");
						break;
					}
				}
				s.sawStraying.clear();
			}
			stage = GetStage(a_actor);
		}

		SetRank(a_actor, f.questBusy, QuestBusy(a_actor) ? 1 : 0);
		SetRank(a_actor, f.band, stage >= Stage::kDating && stage <= Stage::kMarried ? 1 + std::min(4, approval / 20) : 0);
		if (stage == Stage::kEx) {
			SetRank(a_actor, f.why, IRF::Rank(s.why));
			SetRank(a_actor, f.who, IRF::Rank(s.who));
		}

		// pending lines on the reaction quest: her leaving you, word getting back, or a reaction to an event
		int reaction = 0;
		if (s.pendingBreakup != Why::kNone) {
			reaction = IRF::Rank(Event::kBreakupSpeech) + IRF::Rank(s.pendingBreakup);
		} else if (s.confrontPending) {
			reaction = IRF::Rank(Event::kConfront);
		} else if (const auto it = State::Reactions().find(a_actor->GetFormID()); it != State::Reactions().end()) {
			if (it->second.untilDay < today) {
				State::Reactions().erase(it);
			} else {
				reaction = IRF::Rank(it->second.event);
			}
		}
		SetRank(a_actor, f.reaction, silent ? 0 : reaction);
		const bool recent = s.recent != Event::kNone && today - s.recentDay < 100.0f;
		SetRank(a_actor, f.recent, recent && !silent ? IRF::Rank(s.recent) : 0);
		SetRank(a_actor, f.recentDays, recent ? static_cast<int>(today - s.recentDay) : 0);

		// the answers to the player's romance lines (0 hides the line)
		const bool hurt = s.hurt && !silent;
		// get to know her: one question a day, Friend to Married. Only talk, never courting (user, 2026-10-25)
		const bool cold = candidate->attributes.waryStyle == Candidates::WaryStyle::kCold && s.waryUntilDay > today;
		const bool askedToday = today - s.lastKnowDay < 1.0f;
		const bool knowable = stage >= Stage::kFriend && stage <= Stage::kMarried && !silent && !hurt && !cold && !askedToday;
		SetRank(a_actor, f.courtFaction, knowable ? 1 : 0);

		// "Will you go on a date with me?": always there for a Friend (one date at a time); yes from askDateApproval,
		// not yet below it, no if she knows you're taken and won't compete. Once she'd say yes, she hints at it
		const bool locked = s.lockUntilDay > today;
		const bool taken = WontBeCourted(a_actor);
		const bool askable = approval >= r.askDateApproval && GateOpen(*candidate, Stage::kDating) && !locked;
		const bool asking = stage == Stage::kFriend && !silent && !FirstDate::Pending();
		SetRank(a_actor, f.askDate, asking ? (taken ? 3 : askable ? 1 : 2) : 0);
		SetRank(a_actor, f.greet, IRF::Rank(GreetFor(stage, s, asking && askable && !taken)));

		const bool booked = Running(f.wedding) && InAliasOf(a_actor, f.wedding);
		const bool partnerStage = stage >= Stage::kDating && stage <= Stage::kMarried;
		const bool refuseFollow = locked || hurt || booked || s.pendingBreakup != Why::kNone;
		SetRank(a_actor, f.follow, partnerStage && !following && !silent ? (refuseFollow ? 2 : 1) : 0);

		// her mood on top of the stage: the strongest wins
		const bool wary = s.waryUntilDay > today;
		auto       mood = Mood::kNone;
		if (stage >= Stage::kDating && stage <= Stage::kMarried) {
			mood = wary                                              ? Mood::kWary :
			       s.hurt || silent                                  ? Mood::kHurt :
			       s.jealousUntilDay > today                         ? Mood::kJealous :
			       approval < r.strainedBelow                        ? Mood::kStrained :
			                                                           Mood::kNone;
		}
		SetRank(a_actor, f.moodFaction, Rank(mood));

		int kiss = 0;
		if (stage >= Stage::kDating && stage <= Stage::kMarried && !silent) {
			kiss = hurt || wary ? 3 : World::PrivateEnough(a_actor, candidate->attributes.modesty) ? 1 : 2;
		}
		SetRank(a_actor, f.kiss, kiss);
		SetRank(a_actor, f.breakUp, stage >= Stage::kDating && stage <= Stage::kEngaged && !silent ? 1 : 0);
		SetRank(a_actor, f.amends, hurt && !s.unforgivable ? 1 : 0);
		// a bitter ex's wait starts when her silence ends; a pleading one asks you herself meanwhile
		const bool pleading = stage == Stage::kEx && s.pleadsLeft > 0 && s.nextPleadDay <= today;
		const bool winBackable = !s.unforgivable && today - std::max(s.breakupDay, s.silentUntilDay) >= static_cast<float>(r.winBackDays);
		SetRank(a_actor, f.winBack, stage == Stage::kEx && !silent && !pleading ? (winBackable ? 1 : 2) : 0);
		SetRank(a_actor, f.plead, pleading ? 1 : 0);
		const bool secretable = stage >= Stage::kDating && stage <= Stage::kCommitted && !s.secret && !silent && !hurt && !s.confrontPending;
		SetRank(a_actor, f.secret, secretable ? (candidate->attributes.refusesSecret ? 2 : 1) : 0);
		// an evening together: from Dating up, evenings, once a day
		const bool datable = stage >= Stage::kDating && stage <= Stage::kMarried && !silent && !hurt && !cold && today - s.lastDateDay >= 1.0f;
		SetRank(a_actor, f.date, datable && World::Evening() ? 1 : 0);
		const bool gaveToday = today - s.lastGiftDay < 1.0f;  // one gift a day: the line waits for tomorrow
		SetRank(a_actor, f.give, stage >= Stage::kFriend && stage <= Stage::kMarried && !silent && !gaveToday ? 1 : 0);
		SetRank(a_actor, f.confront, s.confrontPending ? (s.confrontWorks ? 1 : 2) : 0);

		// the amulet: below Committed the framework's own line and a rejection; from Committed vanilla's
		const bool rejects = stage >= Stage::kFriend && stage < Stage::kCommitted && !silent;
		SetRank(a_actor, f.amulet, rejects && AmuletOffered() ? IRF::Rank(stage) + 1 : 0);
		const bool mayMarry = stage >= Stage::kCommitted && stage <= Stage::kMarried;
		if (mayMarry && !InFaction(a_actor, f.potentialMarriage)) {
			a_actor->AddToFaction(f.potentialMarriage, 0);
		} else if (!mayMarry && InFaction(a_actor, f.potentialMarriage) && Rank(a_actor, f.potentialMarriage) >= 0) {
			a_actor->RemoveFromFaction(f.potentialMarriage);
		}

		// a booked fiancee stops following and waits for the day
		if (booked && following) {
			Follower::Dismiss(a_actor);
		}
		Follower::SyncProfile(a_actor);
		SyncSpouseBonus(a_actor, *candidate, stage, s);
		SyncChat(a_actor);
	}

	void SyncAll()
	{
		for (const auto& candidate : Candidates::All()) {
			if (const auto actor = Candidates::ActorOf(candidate)) {
				Sync(actor);
				for (const auto& member : candidate.circle) {
					if (const auto other = RE::TESForm::LookupByID<RE::Actor>(member.ref)) {
						SyncOther(other);
					}
				}
			}
		}
	}

	void StartQuests()
	{
		for (const auto quest : Forms::Get().quests) {
			if (!Running(quest)) {
				logger::info("Romance: starting {:08X}, which this save didn't have running", quest->GetFormID());
				quest->Start();
			}
		}
	}
}
