#include "Papyrus.h"

#include "Expose.h"
#include "Family.h"

#include "Candidates.h"
#include "Children.h"
#include "Events.h"
#include "Rules.h"
#include "Follower.h"
#include "Forms.h"
#include "Romance.h"
#include "Evening.h"
#include "FirstDate.h"
#include "Scenes.h"
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

		// IRF_Internal.Approach: the walker has reached the other (1 two partners having it out, 2 a family
		// conversation); the scene goes on, on the game's safe point
		void Arrived(RE::StaticFunctionTag*, std::int32_t a_scene)
		{
			SKSE::GetTaskInterface()->AddTask([a_scene]() {
				if (a_scene == 1) {
					Expose::OnArrived();
				} else if (a_scene == 2) {
					Family::OnArrived();
				}
			});
		}

		// IRF_Internal.EveningTogether: the screen is black (time passes), and then you're both outside
		void EveningTime(RE::StaticFunctionTag*, float a_hours)
		{
			SKSE::GetTaskInterface()->AddTask([a_hours]() { Evening::PassTime(a_hours); });
		}

		void EveningDone(RE::StaticFunctionTag*, RE::Actor* a_her)
		{
			if (a_her) {
				SKSE::GetTaskInterface()->AddTask([handle = a_her->GetHandle()]() {
					if (const auto her = handle.get()) {
						Evening::Done(her.get());
					}
				});
			}
		}

		// IRF_Internal.DateFade: the first date's fade is over
		void DateFaded(RE::StaticFunctionTag*, RE::Actor* a_her)
		{
			if (a_her) {
				SKSE::GetTaskInterface()->AddTask([handle = a_her->GetHandle()]() {
					if (const auto her = handle.get()) {
						FirstDate::Faded(her.get());
					}
				});
			}
		}

		// IRF_Internal.WalkHome: once a second while you walk her home (the check itself runs as a task)
		bool DateWalkTick(RE::StaticFunctionTag*, std::int32_t a_walk)
		{
			return FirstDate::WalkTick(a_walk);
		}

		// IRF_Internal.TurnBack: you're back with her
		void DateTurnedBack(RE::StaticFunctionTag*)
		{
			SKSE::GetTaskInterface()->AddTask([]() { FirstDate::TurnedBack(); });
		}

		// IRF_Internal.BeatLater: the scene director's clock while a scene is on
		void Beat(RE::StaticFunctionTag*, std::int32_t a_beat)
		{
			SKSE::GetTaskInterface()->AddTask([a_beat]() { Scenes::OnBeat(a_beat); });
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
		a_vm->RegisterFunction("Arrived"sv, "IRF_Internal"sv, Arrived);
		a_vm->RegisterFunction("Beat"sv, "IRF_Internal"sv, Beat);
		a_vm->RegisterFunction("EveningTime"sv, "IRF_Internal"sv, EveningTime);
		a_vm->RegisterFunction("EveningDone"sv, "IRF_Internal"sv, EveningDone);
		a_vm->RegisterFunction("DateFaded"sv, "IRF_Internal"sv, DateFaded);
		a_vm->RegisterFunction("DateWalkTick"sv, "IRF_Internal"sv, DateWalkTick);
		a_vm->RegisterFunction("DateTurnedBack"sv, "IRF_Internal"sv, DateTurnedBack);
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
