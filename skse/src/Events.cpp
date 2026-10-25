#include "Events.h"

#include "Candidates.h"
#include "Romance.h"

namespace Events
{
	namespace
	{
		// Game-state changes go through one SKSE task (the safe point); the handle survives until it runs
		void QueueSync(RE::Actor* a_actor)
		{
			SKSE::GetTaskInterface()->AddTask([handle = a_actor->GetHandle()]() {
				if (const auto actor = handle.get()) {
					Romance::Sync(actor.get());
				}
			});
		}

		class Crosshair : public RE::BSTEventSink<SKSE::CrosshairRefEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override
			{
				const auto actor = a_event && a_event->crosshairRef ? a_event->crosshairRef->As<RE::Actor>() : nullptr;
				if (actor && Candidates::Find(actor)) {
					QueueSync(actor);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Menus : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event || !a_event->opening || a_event->menuName != RE::DialogueMenu::MENU_NAME) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto topics = RE::MenuTopicManager::GetSingleton();
				const auto speaker = topics ? topics->speaker.get() : nullptr;
				const auto actor = speaker ? speaker->As<RE::Actor>() : nullptr;
				if (actor && Candidates::Find(actor)) {
					QueueSync(actor);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class TopicInfo : public RE::BSTEventSink<RE::TESTopicInfoEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* a_event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
			{
				if (!a_event || a_event->type != RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd) {
					return RE::BSEventNotifyControl::kContinue;
				}
				std::string key;
				if (!Candidates::FindLine(a_event->topicInfoFormID, &key)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto speaker = a_event->speakerRef ? a_event->speakerRef->As<RE::Actor>() : nullptr;
				if (!speaker) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (key == "court") {
					SKSE::GetTaskInterface()->AddTask([handle = speaker->GetHandle()]() {
						const auto actor = handle.get();
						if (actor && Romance::GetStage(actor.get()) == Romance::Stage::kFriend) {
							Romance::SetStage(actor.get(), Romance::Stage::kCourting, "courted");
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register()
	{
		static Crosshair crosshair;
		static Menus     menus;
		static TopicInfo topicInfo;
		SKSE::GetCrosshairRefEventSource()->AddEventSink(&crosshair);
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menus);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESTopicInfoEvent>(&topicInfo);
		logger::info("Events: crosshair, dialogue menu and topic info sinks registered");
	}
}
