// The debug interface (include/IRFDebugAPI.h): what test tools read and do. Game thread only, like the rest.

#include "IRFDebugAPI.h"

#include "Candidates.h"
#include "Children.h"
#include "Crimes.h"
#include "Events.h"
#include "FirstDate.h"
#include "Follower.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "State.h"
#include "World.h"

namespace
{
	using namespace IRF;

	RE::Actor* CandidateActor(std::uint32_t a_ref)
	{
		const auto actor = RE::TESForm::LookupByID<RE::Actor>(a_ref);
		return actor && Candidates::Find(actor) ? actor : nullptr;
	}

	void Copy(char* a_out, std::uint32_t a_size, std::string_view a_text)
	{
		if (!a_out || a_size == 0) {
			return;
		}
		const auto n = std::min<std::size_t>(a_text.size(), a_size - 1);
		std::memcpy(a_out, a_text.data(), n);
		a_out[n] = '\0';
	}

	int RankIn(RE::Actor* a_actor, RE::TESFaction* a_faction)
	{
		return a_faction && a_actor->IsInFaction(a_faction) ? a_actor->GetFactionRank(a_faction, false) : 0;
	}

	std::uint32_t ListCandidates(IRFDebug::Candidate* a_out, std::uint32_t a_max)
	{
		const auto& all = Candidates::All();
		for (std::uint32_t i = 0; i < a_max && i < all.size(); ++i) {
			a_out[i].ref = all[i].ref;
			Copy(a_out[i].name, sizeof(a_out[i].name), all[i].name);
		}
		return static_cast<std::uint32_t>(all.size());
	}

	bool Read(std::uint32_t a_ref, IRFDebug::State* a_out)
	{
		const auto actor = CandidateActor(a_ref);
		if (!actor || !a_out) {
			return false;
		}
		const auto  candidate = Candidates::Find(actor);
		const auto& f = Forms::Get();
		const auto& s = State::Of(a_ref);
		const auto  today = State::Today();
		auto&       o = *a_out;
		o = {};
		o.stage = Rank(Romance::GetStage(actor));
		o.approval = Romance::GetApproval(actor);
		o.band = RankIn(actor, f.band);
		o.mood = RankIn(actor, f.moodFaction);
		o.greet = RankIn(actor, f.greet);
		o.following = actor->IsPlayerTeammate();
		o.hurt = s.hurt;
		o.silentDays = std::max(0.0f, s.silentUntilDay - today);
		o.who = Rank(s.who);
		o.why = Rank(s.why);
		o.unforgivable = s.unforgivable;
		o.secret = s.secret;
		o.lockDays = std::max(0.0f, s.lockUntilDay - today);
		o.waryDays = std::max(0.0f, s.waryUntilDay - today);
		o.jealousDays = std::max(0.0f, s.jealousUntilDay - today);
		o.contactDaysAgo = s.lastContactDay <= State::kNever ? -1.0f : today - s.lastContactDay;
		o.recent = Rank(s.recent);
		o.recentDaysAgo = today - s.recentDay;
		o.lastDateDaysAgo = today - s.lastDateDay;
		o.lastKissDaysAgo = today - s.lastKissDay;
		o.lastGiftDaysAgo = today - s.lastGiftDay;
		o.pendingBreakup = Rank(s.pendingBreakup);
		o.confrontPending = s.confrontPending;
		o.knowSeeing = static_cast<std::int32_t>(s.knowSeeing.size());
		o.sawStraying = static_cast<std::int32_t>(s.sawStraying.size());
		o.confessed = s.confessed;
		o.knowAsked = s.knowAsked;
		o.firstDatePending = s.dateUntilDay > 0.0f;
		o.dateHoursLeft = o.firstDatePending ? (s.dateUntilDay - today) * 24.0f : 0.0f;
		o.partnerXp = s.partnerXp;
		o.appliedLevels = s.appliedLevels;
		o.boys = Children::Boys();
		o.girls = Children::Girls();
		const auto player = RE::PlayerCharacter::GetSingleton();
		for (const auto& member : candidate->circle) {
			if (o.circleCount >= IRFDebug::kMaxCircle) {
				break;
			}
			auto& c = o.circle[o.circleCount++];
			c.ref = member.ref;
			Copy(c.name, sizeof(c.name), member.name);
			if (const auto it = State::Reactions().find(member.ref); it != State::Reactions().end()) {
				c.reaction = Rank(it->second.event);
				c.reactionDays = std::max(0.0f, it->second.untilDay - today);
			}
			if (const auto it = State::Refusals().find(member.ref); it != State::Refusals().end() && it->second.because == a_ref) {
				c.refusing = true;
				c.refusalDays = it->second.untilDay > 0.0f ? std::max(0.0f, it->second.untilDay - today) : 0.0f;
			}
			if (const auto other = RE::TESForm::LookupByID<RE::Actor>(member.ref); other && other->GetActorBase() && player->GetActorBase()) {
				const auto rel = RE::BGSRelationship::GetRelationship(other->GetActorBase(), player->GetActorBase());
				c.rankWithPlayer = rel ? 4 - static_cast<std::int32_t>(rel->level.get()) : 0;  // kLover 0 is rank 4 ... kArchnemesis 8 is -4
			}
		}
		return true;
	}

	bool SetStage(std::uint32_t a_ref, std::int32_t a_stage)
	{
		const auto actor = CandidateActor(a_ref);
		if (!actor || a_stage < -1 || a_stage > Rank(Stage::kKilled)) {
			return false;
		}
		const auto married = Forms::Get().playerMarried;
		const auto stage = static_cast<Stage>(a_stage);
		if (married) {
			if (stage == Stage::kMarried) {
				actor->AddToFaction(married, 0);  // vanilla's "married to the player": the framework keeps Married by it
			} else if (actor->IsInFaction(married)) {
				actor->RemoveFromFaction(married);
			}
		}
		Romance::SetStage(actor, stage, "debug");
		Romance::Sync(actor);
		return true;
	}

	bool SetApproval(std::uint32_t a_ref, std::int32_t a_value)
	{
		const auto actor = CandidateActor(a_ref);
		if (!actor) {
			return false;
		}
		Romance::ModApproval(actor, a_value - Romance::GetApproval(actor), "debug");
		Romance::Sync(actor);
		return true;
	}

	bool Deed(std::uint32_t a_ref, const char* a_deed)
	{
		const auto actor = CandidateActor(a_ref);
		if (!actor || !a_deed) {
			return false;
		}
		const auto        candidate = Candidates::Find(actor);
		const std::string deed = a_deed;
		auto&             s = State::Of(a_ref);
		if (deed == "caught") {
			Romance::Caught(actor, "debug");
		} else if (deed == "bleedout") {
			Romance::Hurt(actor, candidate->attributes.bleedout, Why::kBleedouts, "debug bleedout");
		} else if (deed == "attack") {
			Events::Attack(actor, "debug attack");  // as a real one: approval, lock, her wary mood
		} else if (deed == "dumped") {
			Romance::BreakUp(actor, Who::kPlayer, Why::kChoice);  // you end it (her heartbreak style applies)
		} else if (deed.starts_with("line:")) {
			Events::TestLine(actor, deed.substr(5));  // as if she had said that line ("line:kiss.yes", "line:date")
		} else if (deed == "murder") {
			Crimes::Witnessed(actor, RE::PackageNS::CRIME_TYPES::kMurder);  // she saw you murder someone
		} else if (deed.starts_with("moment:")) {
			World::SayMoment(actor, static_cast<Moment>(std::atoi(deed.c_str() + 7)));  // her one-liner for that moment ("moment:6" jealous)
		} else if (deed == "jealous") {
			s.jealousUntilDay = std::floor(State::Today()) + static_cast<float>(Rules::Get().jealousDays);
		} else if (deed == "absence") {
			s.lastContactDay = State::Today() - static_cast<float>(candidate->attributes.absenceDays) * 3.0f;
		} else if (deed == "crime") {
			Romance::Hurt(actor, { std::abs(candidate->attributes.alignment) * 4, false }, Why::kCrime, "debug crime");
		} else if (deed == "breakup") {
			s.pendingBreakup = Why::kAbsence;
		} else if (deed == "amends") {
			s.hurt = false;
			s.silentUntilDay = 0.0f;
			Romance::Notify(actor, Event::kAmends);
		} else if (deed == "xp") {
			Follower::AddXp(actor, 100, "debug");
		} else {
			return false;
		}
		Romance::Sync(actor);
		return true;
	}

	bool SkipDays(std::uint32_t a_ref, std::int32_t a_days)
	{
		const auto actor = RE::TESForm::LookupByID<RE::Actor>(a_ref);
		if (!actor) {
			return false;
		}
		const auto d = static_cast<float>(a_days);
		if (Children::IsChild(actor)) {  // an adopted child: when they last saw you
			auto& c = State::Children()[a_ref];
			c.lastSeenDay = (c.lastSeenDay <= State::kNever ? State::Today() : c.lastSeenDay) - d;
			c.lastMoodDay -= d;
			Children::Sync(actor);
			return true;
		}
		if (!Candidates::Find(actor)) {
			return false;
		}
		auto& s = State::Of(a_ref);
		s.lastContactDay -= d;
		s.silentUntilDay -= d;
		s.breakupDay -= d;
		s.lockUntilDay -= d;
		s.waryUntilDay -= d;
		s.jealousUntilDay -= d;
		s.nextPleadDay -= d;
		if (s.readyDay != State::kNever) {
			s.readyDay -= d;
		}
		s.recentDay -= d;
		s.lastKnowDay -= d;
		if (s.dateUntilDay > 0.0f) {
			s.dateUntilDay -= d;  // a date that's now in the past: stood up at the next check
		}
		s.lastDateDay -= d;
		s.lastKissDay -= d;
		s.lastGiftDay -= d;
		for (auto& [id, refusal] : State::Refusals()) {
			if (refusal.untilDay > 0.0f) {
				refusal.untilDay -= d;
			}
		}
		Romance::Sync(actor);
		FirstDate::Check();
		return true;
	}

	bool Reset(std::uint32_t a_ref)
	{
		const auto actor = CandidateActor(a_ref);
		if (!actor) {
			return false;
		}
		State::All().erase(a_ref);
		Romance::SetStage(actor, Stage::kNone, "debug reset");
		actor->RemoveFromFaction(Forms::Get().approval);
		Romance::Sync(actor);
		return true;
	}

	bool Adopt(std::uint32_t a_childRef)
	{
		const auto  child = RE::TESForm::LookupByID<RE::Actor>(a_childRef);
		const auto& f = Forms::Get();
		if (!child || !f.adoption || !f.adoptionFaction) {
			return false;  // or Hearthfire isn't loaded
		}
		for (const auto alias : f.adoption->aliases) {
			if (!alias || alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID || (alias->aliasName != "Child1"sv && alias->aliasName != "Child2"sv)) {
				continue;
			}
			const auto ref = static_cast<RE::BGSRefAlias*>(alias);
			if (!ref->GetReference()) {
				ref->ForceRefTo(child);
				child->AddToFaction(f.adoptionFaction, 0);
				logger::info("Debug: {} counts as adopted ({})", child->GetDisplayFullName(), alias->aliasName.c_str());
				Children::Count();
				Children::Sync(child);
				return true;
			}
		}
		return false;  // both child aliases are taken
	}

	bool QueueReaction(std::uint32_t a_memberRef, std::uint32_t a_candidateRef, std::int32_t a_event)
	{
		const auto member = RE::TESForm::LookupByID<RE::Actor>(a_memberRef);
		if (!member || !CandidateActor(a_candidateRef) || a_event <= 0) {
			return false;
		}
		// as Romance::Notify does for one person, without the rest of the event (the recent line, refusals, others)
		State::Reactions()[a_memberRef] = { static_cast<Event>(a_event), State::Today() + static_cast<float>(Rules::Get().reactionDays) };
		Romance::SyncOther(member);
		return true;
	}

	bool StartFirstDate(std::uint32_t a_ref)
	{
		const auto actor = CandidateActor(a_ref);
		if (!actor || FirstDate::Pending()) {
			return false;
		}
		FirstDate::Start(actor);
		return FirstDate::Pending();
	}

	bool LineKey(std::uint32_t a_info, char* a_key, std::uint32_t a_size, std::uint32_t* a_candidateRef)
	{
		std::string key;
		const auto  candidate = Candidates::FindLine(a_info, &key);
		if (!candidate) {
			return false;
		}
		Copy(a_key, a_size, key);
		if (a_candidateRef) {
			*a_candidateRef = candidate->ref;
		}
		return true;
	}

	bool EventNameOf(std::int32_t a_event, char* a_out, std::uint32_t a_size)
	{
		const auto name = Candidates::EventName(static_cast<Event>(a_event));
		Copy(a_out, a_size, name);
		return !name.empty();
	}

	std::int32_t EventByName(const char* a_name)
	{
		for (std::int32_t e = 1; a_name && e < 64; ++e) {
			if (Candidates::EventName(static_cast<Event>(e)) == a_name) {
				return e;
			}
		}
		return 0;
	}

	bool CheckJealous(std::uint32_t a_ref, char* a_why, std::uint32_t a_size)
	{
		std::string what;
		const bool  jealous = Events::TestJealous(RE::TESForm::LookupByID<RE::Actor>(a_ref), what);
		Copy(a_why, a_size, what);
		return jealous;
	}

	bool SetRule(const char* a_name, const char* a_json)
	{
		return a_name && a_json && Rules::Set(a_name, a_json);
	}

	const IRFDebug::API g_api{
		IRFDebug::kVersion,
		ListCandidates,
		Read,
		SetStage,
		SetApproval,
		Deed,
		SkipDays,
		Reset,
		Adopt,
		QueueReaction,
		StartFirstDate,
		LineKey,
		EventNameOf,
		EventByName,
		CheckJealous,
		SetRule,
	};
}

extern "C" __declspec(dllexport) const IRFDebug::API* IRFGetDebugAPI()
{
	return &g_api;
}
