#include "Papyrus.h"

#include "Candidates.h"
#include "Romance.h"

namespace Papyrus
{
	namespace
	{
		using VM = RE::BSScript::IVirtualMachine;

		bool IsCandidate(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Candidates::Find(a_actor) != nullptr;
		}

		std::int32_t GetStage(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Candidates::Find(a_actor) ? static_cast<std::int32_t>(Romance::GetStage(a_actor)) : -1;
		}

		std::int32_t GetApproval(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Candidates::Find(a_actor) ? Romance::GetApproval(a_actor) : -1;
		}

		// A stage move asked for by a quest (a story beat); the framework carries it out
		void RequestStage(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_stage, RE::BSFixedString a_cause)
		{
			if (!a_actor || a_stage < -1 || a_stage > static_cast<std::int32_t>(Romance::Stage::kMarried)) {
				return;
			}
			std::string cause = a_cause.empty() ? "quest"s : a_cause.c_str();
			SKSE::GetTaskInterface()->AddTask([handle = a_actor->GetHandle(), a_stage, cause]() {
				if (const auto actor = handle.get()) {
					Romance::SetStage(actor.get(), static_cast<Romance::Stage>(a_stage), cause);
				}
			});
		}

		void ModApproval(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_amount, RE::BSFixedString a_cause)
		{
			if (!a_actor) {
				return;
			}
			std::string cause = a_cause.empty() ? "script"s : a_cause.c_str();
			SKSE::GetTaskInterface()->AddTask([handle = a_actor->GetHandle(), a_amount, cause]() {
				if (const auto actor = handle.get()) {
					Romance::ModApproval(actor.get(), a_amount, cause);
				}
			});
		}
	}

	bool Register(VM* a_vm)
	{
		constexpr auto script = "IRF"sv;
		a_vm->RegisterFunction("IsCandidate"sv, script, IsCandidate);
		a_vm->RegisterFunction("GetStage"sv, script, GetStage);
		a_vm->RegisterFunction("GetApproval"sv, script, GetApproval);
		a_vm->RegisterFunction("RequestStage"sv, script, RequestStage);
		a_vm->RegisterFunction("ModApproval"sv, script, ModApproval);
		logger::info("Papyrus: IRF natives registered");
		return true;
	}
}
