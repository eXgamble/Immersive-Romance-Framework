#include "Replies.h"

#include "Forms.h"
#include "World.h"

namespace Replies
{
	using namespace IRF;

	namespace
	{
		constexpr float kHearing = 1500.0f;  // about 21 m: farther off, she didn't hear it

		struct Pending
		{
			RE::FormID      candidate{ 0 };
			RE::ActorHandle speaker;
			int             reply{ 0 };
		};
		Pending g_pending;  // game thread (SKSE tasks): a reply waiting for the talk to end

		void Say(const Pending& a_pending)
		{
			const auto actor = RE::TESForm::LookupByID<RE::Actor>(a_pending.candidate);
			const auto speaker = a_pending.speaker.get();
			if (!actor || !speaker || actor->IsDead() || !actor->Is3DLoaded() || actor->IsInCombat() ||
				actor->GetPosition().GetDistance(speaker->GetPosition()) > kHearing) {
				return;
			}
			actor->AddToFaction(Forms::Get().reply, static_cast<std::int8_t>(a_pending.reply));
			World::SayMoment(actor, Moment::kReply, 0.5f);
			logger::info("Replies: {} answers {} (reply {})", actor->GetDisplayFullName(), speaker->GetDisplayFullName(), a_pending.reply);
		}
	}

	void OnLineEnded(RE::Actor* a_speaker, RE::FormID a_candidate, int a_reply)
	{
		const Pending pending{ a_candidate, a_speaker->GetHandle(), a_reply };
		if (RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
			g_pending = pending;  // a line said while the dialogue menu is open never plays (measured 2026-10-25)
			return;
		}
		Say(pending);
	}

	void OnDialogueClosed()
	{
		if (g_pending.reply > 0) {
			const auto pending = g_pending;
			g_pending = {};
			Say(pending);
		}
	}

	void OnGameLoaded()
	{
		g_pending = {};
	}
}
