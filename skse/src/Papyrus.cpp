#include "Papyrus.h"

#include "Expose.h"

#include "Candidates.h"
#include "Children.h"
#include "Events.h"
#include "Rules.h"
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

		// IRF_Internal.ExposeApproach: A has walked up to B (the scene goes on, on the game's safe point)
		void ExposeArrived(RE::StaticFunctionTag*)
		{
			SKSE::GetTaskInterface()->AddTask([]() { Expose::OnArrived(); });
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
			if (stage < Stage::kDating || stage > Stage::kMarried || s.hurt || s.silentUntilDay > State::Today()) {
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

		std::int32_t GetAdoptedBoys(RE::StaticFunctionTag*)
		{
			return Children::Boys();
		}

		std::int32_t GetAdoptedGirls(RE::StaticFunctionTag*)
		{
			return Children::Girls();
		}
	}

	bool Register(VM* a_vm)
	{
		constexpr auto script = "IRF"sv;
		a_vm->RegisterFunction("ExposeArrived"sv, "IRF_Internal"sv, ExposeArrived);
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
		a_vm->RegisterFunction("GetAdoptedBoys"sv, script, GetAdoptedBoys);
		a_vm->RegisterFunction("GetAdoptedGirls"sv, script, GetAdoptedGirls);
		logger::info("Papyrus: IRF natives registered");
		return true;
	}
}
