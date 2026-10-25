#pragma once

// Calls into vanilla Papyrus scripts the framework relies on (say a topic, the follower system, quest stages, a
// screen fade). Fire and forget; call from an SKSE task.
namespace Script
{
	template <class... Args>
	void Call(RE::TESForm* a_form, std::string_view a_class, std::string_view a_function, Args... a_args)
	{
		if (!a_form) {
			return;
		}
		const auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		const auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		if (!policy) {
			return;
		}
		const auto handle = policy->GetHandleForObject(a_form->GetFormType(), a_form);
		if (handle == policy->EmptyHandle()) {
			logger::warn("Script: no handle for {:08X} to call {}.{}", a_form->GetFormID(), a_class, a_function);
			return;
		}
		auto args = RE::MakeFunctionArguments(std::move(a_args)...);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		vm->DispatchMethodCall(handle, RE::BSFixedString(a_class), RE::BSFixedString(a_function), args, callback);
	}

	template <class... Args>
	void CallStatic(std::string_view a_class, std::string_view a_function, Args... a_args)
	{
		const auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}
		auto args = RE::MakeFunctionArguments(std::move(a_args)...);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		if (!vm->DispatchStaticCall(RE::BSFixedString(a_class), RE::BSFixedString(a_function), args, callback)) {
			logger::warn("Script: could not call {}.{}", a_class, a_function);
		}
	}

	// The speaker says the line of a_topic whose conditions pass (ObjectReference.Say)
	inline void Say(RE::Actor* a_speaker, RE::TESTopic* a_topic)
	{
		Call(a_speaker, "ObjectReference"sv, "Say"sv, a_topic, static_cast<RE::Actor*>(nullptr), false);
	}
}
