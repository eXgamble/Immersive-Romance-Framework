#include "Papyrus.h"

#include "Candidates.h"
#include "Children.h"
#include "Follower.h"
#include "Forms.h"
#include "Romance.h"
#include "State.h"
#include "World.h"

namespace Papyrus
{
	using namespace IRF;

	namespace
	{
		using VM = RE::BSScript::IVirtualMachine;

		template <class F>
		void Later(RE::Actor* a_actor, F&& a_fn)
		{
			if (!a_actor) {
				return;
			}
			SKSE::GetTaskInterface()->AddTask([handle = a_actor->GetHandle(), fn = std::forward<F>(a_fn)]() {
				if (const auto actor = handle.get()) {
					fn(actor.get());
				}
			});
		}

		bool IsCandidate(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Candidates::Find(a_actor) != nullptr;
		}

		std::int32_t GetStage(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Candidates::Find(a_actor) ? Rank(Romance::GetStage(a_actor)) : -1;
		}

		std::int32_t GetApproval(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Candidates::Find(a_actor) ? Romance::GetApproval(a_actor) : -1;
		}

		RE::Actor* GetPartner(RE::StaticFunctionTag*)
		{
			return Romance::Partner();
		}

		std::int32_t GetWhy(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return a_actor && Candidates::Find(a_actor) ? Rank(State::Of(a_actor->GetFormID()).why) : 0;
		}

		std::int32_t GetWho(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return a_actor && Candidates::Find(a_actor) ? Rank(State::Of(a_actor->GetFormID()).who) : 0;
		}

		bool IsBrokenHearted(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			if (!a_actor || !Candidates::Find(a_actor)) {
				return false;
			}
			const auto& s = State::Of(a_actor->GetFormID());
			return s.hurt || s.silentUntilDay > State::Today();
		}

		// "Would they kiss / be intimate here?" for other mods (OStim scene starters and the like): 0 = kiss, 1 = intimate
		bool IsIntimacyAllowed(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_level)
		{
			const auto candidate = Candidates::Find(a_actor);
			if (!candidate) {
				return true;  // not ours to judge
			}
			const auto stage = Romance::GetStage(a_actor);
			const auto& s = State::Of(a_actor->GetFormID());
			if (stage < Stage::kCourting || stage > Stage::kMarried || s.hurt || s.silentUntilDay > State::Today()) {
				return false;
			}
			const int modesty = std::min(3, candidate->attributes.modesty + (a_level > 0 ? 1 : 0));
			return a_level == 0 ? World::PrivateEnough(a_actor, modesty) : stage >= Stage::kDating && World::PrivateEnough(a_actor, modesty);
		}

		void RequestStage(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_stage, RE::BSFixedString a_cause)
		{
			if (a_stage < -1 || a_stage > Rank(Stage::kKilled)) {
				return;
			}
			std::string cause = a_cause.empty() ? "quest"s : a_cause.c_str();
			Later(a_actor, [a_stage, cause](RE::Actor* a) { Romance::SetStage(a, static_cast<Stage>(a_stage), cause); });
		}

		void ModApproval(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_amount, RE::BSFixedString a_cause)
		{
			std::string cause = a_cause.empty() ? "script"s : a_cause.c_str();
			Later(a_actor, [a_amount, cause](RE::Actor* a) { Romance::ModApproval(a, a_amount, cause); });
		}

		void SetPlayerInterest(RE::StaticFunctionTag*, RE::BSFixedString a_interest)
		{
			State::PlayerInterest() = a_interest.c_str();
			SKSE::GetTaskInterface()->AddTask([]() { Romance::SyncAll(); });
		}

		// ---- debug tools (dev and authors: stage, approval, deeds, days, a state dump) ----

		void DebugSetApproval(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_value)
		{
			Later(a_actor, [a_value](RE::Actor* a) { Romance::ModApproval(a, a_value - Romance::GetApproval(a), "debug"); });
		}

		// Moves this candidate's clocks back, as if a_days had passed (absence, broken heart, win-back, refusals)
		void DebugSkipDays(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_days)
		{
			Later(a_actor, [a_days](RE::Actor* a) {
				const auto d = static_cast<float>(a_days);
				if (Children::IsChild(a)) {  // an adopted child: when they last saw you
					auto& c = State::Children()[a->GetFormID()];
					c.lastSeenDay = (c.lastSeenDay <= State::kNever ? State::Today() : c.lastSeenDay) - d;
					c.lastMoodDay -= d;
					logger::info("Debug: {}'s clocks moved back {} days", a->GetDisplayFullName(), a_days);
					Children::Sync(a);
					return;
				}
				auto& s = State::Of(a->GetFormID());
				s.lastContactDay -= d;
				s.silentUntilDay -= d;
				s.breakupDay -= d;
				s.lockUntilDay -= d;
				s.recentDay -= d;
				s.lastTalkDay -= d;
				s.lastDateDay -= d;
				s.lastKissDay -= d;
				s.lastGiftDay -= d;
				for (auto& [id, refusal] : State::Refusals()) {
					if (refusal.untilDay > 0.0f) {
						refusal.untilDay -= d;
					}
				}
				logger::info("Debug: {}'s clocks moved back {} days", a->GetDisplayFullName(), a_days);
				Romance::Sync(a);
			});
		}

		// "caught", "bleedout", "attack", "absence", "crime", "breakup" (they end it), "amends", "xp"
		void DebugDeed(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_deed)
		{
			std::string deed = a_deed.c_str();
			Later(a_actor, [deed](RE::Actor* a) {
				const auto candidate = Candidates::Find(a);
				if (!candidate) {
					return;
				}
				auto& s = State::Of(a->GetFormID());
				if (deed == "caught") {
					Romance::Caught(a, "debug");
				} else if (deed == "bleedout") {
					Romance::Hurt(a, candidate->attributes.bleedout, Why::kBleedouts, "debug bleedout");
				} else if (deed == "attack") {
					Romance::Hurt(a, candidate->attributes.hit, Why::kViolence, "debug attack", 10);
				} else if (deed == "absence") {
					s.lastContactDay = State::Today() - static_cast<float>(candidate->attributes.absenceDays) * 3.0f;
				} else if (deed == "crime") {
					Romance::Hurt(a, { std::abs(candidate->attributes.alignment) * 4, false }, Why::kCrime, "debug crime");
				} else if (deed == "breakup") {
					s.pendingBreakup = Why::kAbsence;
				} else if (deed == "amends") {
					s.hurt = false;
					s.silentUntilDay = 0.0f;
					Romance::Notify(a, Event::kAmends);
				} else if (deed == "xp") {
					Follower::AddXp(a, 100, "debug");
				}
				Romance::Sync(a);
			});
		}

		void DebugDump(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Later(a_actor, [](RE::Actor* a) {
				const auto candidate = Candidates::Find(a);
				if (!candidate) {
					logger::info("Debug: {} is not a candidate", a->GetDisplayFullName());
					return;
				}
				const auto& s = State::Of(a->GetFormID());
				const auto  today = State::Today();
				logger::info("Debug: {} stage {} approval {} | silent {:.1f}d hurt {} | ex who {} why {} unforgivable {} | secret {} | lock {:.1f}d | "
							 "contact {:.1f}d ago | recent {} {:.1f}d ago | xp {} levels {} | pending breakup {} confront {} | knowSeeing {} sawStraying {}",
					a->GetDisplayFullName(), Rank(Romance::GetStage(a)), Romance::GetApproval(a), std::max(0.0f, s.silentUntilDay - today), s.hurt,
					Rank(s.who), Rank(s.why), s.unforgivable, s.secret, std::max(0.0f, s.lockUntilDay - today), today - s.lastContactDay, Rank(s.recent),
					today - s.recentDay, s.partnerXp, s.appliedLevels, Rank(s.pendingBreakup), s.confrontPending, s.knowSeeing.size(), s.sawStraying.size());
				RE::SendHUDMessage::ShowHUDMessage(std::format("{}: stage {}, approval {}", a->GetDisplayFullName(), Rank(Romance::GetStage(a)),
					Romance::GetApproval(a))
														.c_str());
			});
		}

		void DebugReset(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Later(a_actor, [](RE::Actor* a) {
				State::All().erase(a->GetFormID());
				Romance::SetStage(a, Stage::kNone, "debug reset");
				a->RemoveFromFaction(Forms::Get().approval);
				Romance::Sync(a);
			});
		}
	}

	namespace
	{
		// A child counts as adopted (Hearthfire's free Child1/Child2 alias and its adoption faction), without
		// Hearthfire's adoption and move: for testing the children's lines and a spouse staying home
		void DebugAdopt(RE::StaticFunctionTag*, RE::Actor* a_child)
		{
			Later(a_child, [](RE::Actor* a) {
				const auto& f = Forms::Get();
				if (!f.adoption || !f.adoptionFaction) {
					logger::info("Debug: Hearthfire isn't loaded");
					return;
				}
				for (const auto alias : f.adoption->aliases) {
					if (!alias || alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID || (alias->aliasName != "Child1"sv && alias->aliasName != "Child2"sv)) {
						continue;
					}
					const auto ref = static_cast<RE::BGSRefAlias*>(alias);
					if (!ref->GetReference()) {
						ref->ForceRefTo(a);
						a->AddToFaction(f.adoptionFaction, 0);
						logger::info("Debug: {} counts as adopted ({})", a->GetDisplayFullName(), alias->aliasName.c_str());
						Children::Sync(a);
						return;
					}
				}
				logger::info("Debug: no free child alias for {}", a->GetDisplayFullName());
			});
		}
	}

	bool Register(VM* a_vm)
	{
		constexpr auto script = "IRF"sv;
		a_vm->RegisterFunction("IsCandidate"sv, script, IsCandidate);
		a_vm->RegisterFunction("GetStage"sv, script, GetStage);
		a_vm->RegisterFunction("GetApproval"sv, script, GetApproval);
		a_vm->RegisterFunction("GetPartner"sv, script, GetPartner);
		a_vm->RegisterFunction("GetWhy"sv, script, GetWhy);
		a_vm->RegisterFunction("GetWho"sv, script, GetWho);
		a_vm->RegisterFunction("IsBrokenHearted"sv, script, IsBrokenHearted);
		a_vm->RegisterFunction("IsIntimacyAllowed"sv, script, IsIntimacyAllowed);
		a_vm->RegisterFunction("RequestStage"sv, script, RequestStage);
		a_vm->RegisterFunction("ModApproval"sv, script, ModApproval);
		a_vm->RegisterFunction("SetPlayerInterest"sv, script, SetPlayerInterest);
		a_vm->RegisterFunction("DebugSetApproval"sv, script, DebugSetApproval);
		a_vm->RegisterFunction("DebugSkipDays"sv, script, DebugSkipDays);
		a_vm->RegisterFunction("DebugDeed"sv, script, DebugDeed);
		a_vm->RegisterFunction("DebugDump"sv, script, DebugDump);
		a_vm->RegisterFunction("DebugReset"sv, script, DebugReset);
		a_vm->RegisterFunction("DebugAdopt"sv, script, DebugAdopt);
		logger::info("Papyrus: IRF natives registered");
		return true;
	}
}
