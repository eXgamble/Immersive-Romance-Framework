#include "Events.h"

#include "Candidates.h"
#include "Follower.h"
#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "State.h"
#include "World.h"

namespace Events
{
	using namespace IRF;
	using Clock = std::chrono::steady_clock;

	namespace
	{
		// Game-state changes go through one SKSE task (the safe point); handles survive until it runs
		template <class F>
		void Queue(F&& a_fn)
		{
			SKSE::GetTaskInterface()->AddTask(std::forward<F>(a_fn));
		}

		RE::Actor* FollowingPartner()
		{
			const auto partner = Romance::Partner();
			return partner && partner->IsPlayerTeammate() ? partner : nullptr;
		}

		bool IsCandidateAt(RE::Actor* a_actor, Stage a_from, Stage a_to)
		{
			if (!Candidates::Find(a_actor)) {
				return false;
			}
			const auto stage = Romance::GetStage(a_actor);
			return stage >= a_from && stage <= a_to;
		}

		// One road line at a time, and the same place not twice in a row
		Clock::time_point g_lastMoment{};
		Moment            g_lastPlace{ Moment::kNone };

		void Moment_(RE::Actor* a_speaker, Moment a_moment)
		{
			const auto now = Clock::now();
			if (now - g_lastMoment < std::chrono::duration<float>(Rules::Get().momentCooldownSeconds)) {
				return;
			}
			g_lastMoment = now;
			World::SayMoment(a_speaker, a_moment);
		}

		// The people around who are candidates learn you're seeing a_partner (a date, a kiss in public, a walk
		// through her town together); a secret relationship doesn't spread this way
		void Spread(RE::Actor* a_partner, bool a_evenIfSecret)
		{
			auto& s = State::Of(a_partner->GetFormID());
			if (s.secret && !a_evenIfSecret) {
				return;
			}
			for (const auto witness : World::Witnesses(RE::PlayerCharacter::GetSingleton(), Rules::Get().witnessDistance)) {
				if (witness != a_partner && Candidates::Find(witness) && s.knowSeeing.insert(witness->GetFormID()).second) {
					logger::info("Events: {} now knows you're seeing {}", witness->GetDisplayFullName(), a_partner->GetDisplayFullName());
				}
			}
		}

		bool Evening()
		{
			const auto calendar = RE::Calendar::GetSingleton();
			const auto hour = calendar ? calendar->GetHour() : 12.0f;
			return hour >= 18.0f && hour <= 23.5f;
		}

		// What a line does when it has been said
		void OnLineSaid(RE::Actor* a_speaker, const std::string& a_key)
		{
			const auto& r = Rules::Get();
			const auto& f = Forms::Get();
			const auto  today = State::Today();
			const auto  candidate = Candidates::Find(a_speaker);
			auto        clearReaction = [&]() {
                State::Reactions().erase(a_speaker->GetFormID());
                a_speaker->AddToFaction(f.reaction, 0);
			};

			if (a_key.starts_with("reaction.") || a_key.find(".reaction.") != std::string::npos) {
				clearReaction();
				return;
			}
			if (!candidate) {
				return;
			}
			auto& s = State::Of(a_speaker->GetFormID());
			const auto stage = Romance::GetStage(a_speaker);

			if (a_key == "court" && stage == Stage::kFriend) {
				Romance::SetStage(a_speaker, Stage::kCourting, "courted");
				Romance::ModApproval(a_speaker, r.talkGain, "courting talk");
				Romance::Strayed(a_speaker, "courting someone else");
			} else if (a_key == "askdate.yes" && stage == Stage::kCourting) {
				Romance::SetStage(a_speaker, Stage::kDating, "asked to be together");
			} else if (a_key == "follow.yes") {
				Follower::Recruit(a_speaker);
			} else if (a_key == "kiss.yes") {
				World::Kiss(a_speaker);
				if (today - s.lastKissDay >= 1.0f) {
					s.lastKissDay = today;
					Romance::ModApproval(a_speaker, r.kissGain, "a kiss");
				}
				Romance::Contact(a_speaker);
				if (Romance::Partner() == a_speaker) {
					Spread(a_speaker, true);  // a kiss in public spreads even a secret
				} else {
					Romance::Strayed(a_speaker, "a kiss with someone else");
				}
			} else if (a_key == "breakup") {
				Romance::BreakUp(a_speaker, Who::kPlayer, Why::kChoice);
			} else if (a_key == "amends") {
				s.hurt = false;
				Romance::ModApproval(a_speaker, r.amendsApproval, "amends");
				Romance::Notify(a_speaker, Event::kAmends);
			} else if (a_key == "winback.yes" && stage == Stage::kEx) {
				a_speaker->AddToFaction(f.approval, static_cast<std::int8_t>(r.winBackApproval));
				Romance::SetStage(a_speaker, Stage::kDating, "won back");
				Romance::Notify(a_speaker, Event::kWonBack);
			} else if (a_key == "secret.yes") {
				s.secret = true;
			} else if (a_key == "talk") {
				const bool date = stage >= Stage::kDating && Evening();
				if (date && today - s.lastDateDay >= 1.0f) {
					s.lastDateDay = today;
					Romance::ModApproval(a_speaker, r.dateGain, "an evening together");
					if (Romance::Partner() == a_speaker) {
						Spread(a_speaker, false);
					}
				} else if (!date && today - s.lastTalkDay >= 1.0f) {
					s.lastTalkDay = today;
					Romance::ModApproval(a_speaker, r.talkGain, "a talk");
				}
				Romance::Contact(a_speaker);
			} else if (a_key == "deny.works") {
				s.confrontPending = false;
			} else if (a_key == "deny.fails" || a_key == "admit") {
				s.confrontPending = false;
				Romance::Caught(a_speaker, "word got back");
			} else if (a_key.starts_with("breakup.")) {
				const auto why = s.pendingBreakup != Why::kNone ? s.pendingBreakup : Why::kAbsence;
				Romance::BreakUp(a_speaker, Who::kThem, why, s.unforgivable);
			} else if (a_key.starts_with("absence.")) {
				a_speaker->AddToFaction(f.absence, 0);
			}
			Romance::Sync(a_speaker);
		}

		class Crosshair : public RE::BSTEventSink<SKSE::CrosshairRefEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override
			{
				const auto actor = a_event && a_event->crosshairRef ? a_event->crosshairRef->As<RE::Actor>() : nullptr;
				if (actor && !actor->IsPlayerRef()) {
					// up close counts as meeting them: absence and word getting back are worked out now, before the
					// game picks their hello (the dialogue menu opens too late for that, measured 2026-10-25)
					const auto player = RE::PlayerCharacter::GetSingleton();
					const bool meeting = player->GetPosition().GetDistance(actor->GetPosition()) < 400.0f;
					Queue([handle = actor->GetHandle(), meeting]() {
						if (const auto a = handle.get()) {
							Romance::Sync(a.get(), meeting);
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// a refusal line waits for the dialogue menu to close and 1.5 s more (a line said while it is open, or up to
		// about a second after it closes, never plays; measured 2026-10-25)
		RE::ActorHandle g_refusing{};

		class Menus : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening && a_event->menuName == RE::DialogueMenu::MENU_NAME && g_refusing) {
					Queue([handle = g_refusing]() {
						if (const auto a = handle.get()) {
							World::SayMoment(a.get(), Moment::kRefuse, 1.5f);
						}
					});
					g_refusing = {};
				}
				if (!a_event || !a_event->opening) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto topics = RE::MenuTopicManager::GetSingleton();
				const auto speakerRef = topics ? topics->speaker.get() : nullptr;
				const auto speaker = speakerRef ? speakerRef->As<RE::Actor>() : nullptr;
				if (!speaker) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto& name = a_event->menuName;
				if (name == RE::DialogueMenu::MENU_NAME) {
					Queue([handle = speaker->GetHandle()]() {
						if (const auto a = handle.get()) {
							Romance::Sync(a.get(), true);
						}
					});
				} else if ((name == RE::BarterMenu::MENU_NAME || name == RE::TrainingMenu::MENU_NAME) && speaker->IsInFaction(Forms::Get().refuse) &&
						   speaker->GetFactionRank(Forms::Get().refuse, false) > 0) {
					// a circle member refuses you service: the shop or training closes and so does the talk (a line said
					// while the dialogue menu is open never plays, measured 2026-10-25), then they say why
					RE::UIMessageQueue::GetSingleton()->AddMessage(name, RE::UI_MESSAGE_TYPE::kHide, nullptr);
					RE::UIMessageQueue::GetSingleton()->AddMessage(RE::DialogueMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
					g_refusing = speaker->GetHandle();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class TopicInfo : public RE::BSTEventSink<RE::TESTopicInfoEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* a_event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
			{
				// kTopicEnd comes when the line has finished (about 6 s for a silent placeholder line)
				std::string key;
				if (!a_event || !Candidates::FindLine(a_event->topicInfoFormID, &key)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				logger::info("Events: line '{}' {} ({:08X})", key, a_event->type == RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd ? "ended" : "began",
					a_event->topicInfoFormID);
				if (a_event->type != RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto speaker = a_event->speakerRef ? a_event->speakerRef->As<RE::Actor>() : nullptr;
				if (speaker) {
					Queue([handle = speaker->GetHandle(), key]() {
						if (const auto a = handle.get()) {
							OnLineSaid(a.get(), key);
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Container : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				if (!a_event || a_event->oldContainer != 0x14 || a_event->itemCount <= 0 || !Candidates::FindByRef(a_event->newContainer)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([to = a_event->newContainer, item = a_event->baseObj]() {
					const auto actor = RE::TESForm::LookupByID<RE::Actor>(to);
					const auto candidate = Candidates::FindByRef(to);
					if (!actor || !candidate || Romance::GetStage(actor) < Stage::kFriend) {
						return;
					}
					const auto& loved = candidate->attributes.lovedGifts;
					const bool  love = std::ranges::any_of(loved, [&](RE::TESForm* a_form) { return a_form->GetFormID() == item; });
					Romance::ModApproval(actor, Rules::Get().giftGain * (love ? 2 : 1), love ? "a gift they love" : "a gift");
					Romance::Contact(actor);
					Romance::Strayed(actor, "a gift for someone else");
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// bleedouts: when a following partner went down, and whether you got them up quickly
		std::unordered_map<RE::FormID, Clock::time_point> g_down;

		void CheckRevive(RE::Actor* a_actor)
		{
			const auto it = g_down.find(a_actor->GetFormID());
			if (it == g_down.end() || a_actor->AsActorState()->IsBleedingOut()) {
				return;
			}
			const auto seconds = std::chrono::duration<float>(Clock::now() - it->second).count();
			g_down.erase(it);
			if (!a_actor->IsDead() && seconds <= Rules::Get().reviveSeconds) {
				Romance::ModApproval(a_actor, Rules::Get().reviveBack, "you got them back up quickly");
				World::SayMoment(a_actor, Moment::kRevive);
			}
		}

		class Bleedout : public RE::BSTEventSink<RE::TESEnterBleedoutEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEnterBleedoutEvent* a_event, RE::BSTEventSource<RE::TESEnterBleedoutEvent>*) override
			{
				const auto actor = a_event && a_event->actor ? a_event->actor->As<RE::Actor>() : nullptr;
				if (!actor || !actor->IsPlayerTeammate() || !Candidates::Find(actor)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([handle = actor->GetHandle()]() {
					const auto a = handle.get();
					const auto candidate = a ? Candidates::Find(a.get()) : nullptr;
					if (!candidate || Romance::GetStage(a.get()) == Stage::kMarried) {
						return;  // after the wedding, bleedouts no longer cost approval
					}
					g_down[a->GetFormID()] = Clock::now();
					Romance::Hurt(a.get(), candidate->attributes.bleedout, Why::kBleedouts, "went down beside you");
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// hits on a partner: stray ones in a fight, or an attack
		std::unordered_map<RE::FormID, Clock::time_point> g_lastHit;

		void Attack(RE::Actor* a_actor, std::string_view a_how)
		{
			const auto candidate = Candidates::Find(a_actor);
			auto&      s = State::Of(a_actor->GetFormID());
			if (s.lockUntilDay > State::Today()) {
				return;  // already counted
			}
			s.strayHits = 0;
			s.lockUntilDay = State::Today() + static_cast<float>(candidate->attributes.attackLockDays);
			logger::info("Events: you attacked {} ({})", a_actor->GetDisplayFullName(), a_how);
			World::SayMoment(a_actor, Moment::kAttacked);
			Follower::Dismiss(a_actor);
			auto tolerance = candidate->attributes.hit;
			if (!tolerance.never) {
				tolerance.cost = Rules::Get().attackCost;
			}
			Romance::Hurt(a_actor, tolerance, Why::kViolence, a_how);
		}

		class Hit : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !a_event->target || !a_event->cause) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto target = a_event->target->As<RE::Actor>();
				if (!target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (target->IsPlayerRef()) {
					// nearly dead: the partner beside you says so
					Queue([]() {
						const auto player = RE::PlayerCharacter::GetSingleton();
						const auto av = player->AsActorValueOwner();
						const auto max = player->GetActorValueMax(RE::ActorValue::kHealth);
						if (max > 0.0f && av->GetActorValue(RE::ActorValue::kHealth) / max < 0.2f) {
							if (const auto partner = FollowingPartner()) {
								Moment_(partner, Moment::kNearlyDead);
							}
						}
					});
					return RE::BSEventNotifyControl::kContinue;
				}
				if (!a_event->cause->IsPlayerRef() || !IsCandidateAt(target, Stage::kCourting, Stage::kMarried)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				// one swing or a concentration spell raises many events: one per second
				const auto now = Clock::now();
				auto&      last = g_lastHit[target->GetFormID()];
				if (now - last < std::chrono::seconds(1)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				last = now;
				const auto& f = Forms::Get();
				const bool  unarmed = a_event->source == 0x1F4 && a_event->projectile == 0;
				if (unarmed && target->IsInFaction(f.brawl)) {
					return RE::BSEventNotifyControl::kContinue;  // a brawl they agreed to
				}
				const bool inFight = target->IsInCombat() && RE::PlayerCharacter::GetSingleton()->IsInCombat();
				Queue([handle = target->GetHandle(), inFight]() {
					const auto a = handle.get();
					const auto candidate = a ? Candidates::Find(a.get()) : nullptr;
					if (!candidate) {
						return;
					}
					if (!inFight) {
						Attack(a.get(), "a hit outside a fight");
						return;
					}
					auto& s = State::Of(a->GetFormID());
					++s.strayHits;
					Romance::Hurt(a.get(), candidate->attributes.hit, Why::kViolence, "a stray hit");
					if (s.strayHits >= candidate->attributes.strayHits) {
						Attack(a.get(), "too many stray hits");
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Combat : public RE::BSTEventSink<RE::TESCombatEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*) override
			{
				const auto actor = a_event && a_event->actor ? a_event->actor->As<RE::Actor>() : nullptr;
				if (!actor || !Candidates::Find(actor)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto state = a_event->newState.get();
				const bool atPlayer = a_event->targetActor && a_event->targetActor->IsPlayerRef();
				Queue([handle = actor->GetHandle(), state, atPlayer]() {
					const auto a = handle.get();
					if (!a) {
						return;
					}
					if (state == RE::ACTOR_COMBAT_STATE::kCombat && atPlayer && IsCandidateAt(a.get(), Stage::kCourting, Stage::kMarried)) {
						Attack(a.get(), "turned on you");
					} else if (state == RE::ACTOR_COMBAT_STATE::kNone) {
						State::Of(a->GetFormID()).strayHits = 0;
						CheckRevive(a.get());
						if (a->IsPlayerTeammate() && !RE::PlayerCharacter::GetSingleton()->IsInCombat()) {
							Moment_(a.get(), Moment::kAfterFight);
						}
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Death : public RE::BSTEventSink<RE::TESDeathEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override
			{
				if (!a_event || a_event->dead || !a_event->actorDying) {
					return RE::BSEventNotifyControl::kContinue;  // react to the killing blow (dead = false)
				}
				const auto dying = a_event->actorDying->As<RE::Actor>();
				const auto killer = a_event->actorKiller ? a_event->actorKiller->As<RE::Actor>() : nullptr;
				if (!dying) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([dyingHandle = dying->GetHandle(), killerHandle = killer ? killer->GetHandle() : RE::ActorHandle{}]() {
					const auto d = dyingHandle.get();
					const auto k = killerHandle.get();
					if (!d) {
						return;
					}
					if (Candidates::Find(d.get())) {
						if (k && k->IsPlayerRef()) {
							auto& s = State::Of(d->GetFormID());
							s.who = Who::kPlayer;
							s.why = Why::kViolence;
						}
						Romance::Sync(d.get());
						return;
					}
					const auto partner = FollowingPartner();
					if (!partner || !k || (!k->IsPlayerRef() && k.get() != partner)) {
						return;
					}
					auto&       s = State::Of(partner->GetFormID());
					const auto& r = Rules::Get();
					const auto  today = State::Today();
					if (today - s.killDay >= 1.0f) {
						s.killDay = today;
						s.killsToday = 0;
					}
					if (s.killsToday < r.killCapPerDay) {
						++s.killsToday;
						Romance::ModApproval(partner, r.killGain, "fighting together");
					}
					Follower::AddXp(partner, r.partnerXpKill * std::max(1, static_cast<int>(d->GetLevel()) / 10), "a kill");
					const auto race = d->GetRace();
					if (race && race->HasKeyword(Forms::Get().dragon)) {
						Moment_(partner, Moment::kDragon);
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Cleared : public RE::BSTEventSink<RE::LocationCleared::Event>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::LocationCleared::Event*, RE::BSTEventSource<RE::LocationCleared::Event>*) override
			{
				Queue([]() {
					if (const auto partner = FollowingPartner()) {
						Romance::ModApproval(partner, Rules::Get().clearGain, "cleared a place together");
						Follower::AddXp(partner, Rules::Get().partnerXpClear, "a cleared place");
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class LocationChange : public RE::BSTEventSink<RE::TESActorLocationChangeEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* a_event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override
			{
				if (!a_event || !a_event->actor || !a_event->actor->IsPlayerRef() || !a_event->newLoc) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([loc = a_event->newLoc]() {
					const auto partner = FollowingPartner();
					if (!partner) {
						return;
					}
					const auto candidate = Candidates::Find(partner);
					Romance::Contact(partner);
					const auto moment = World::MomentForPlace(loc, candidate->community);
					if (moment != Moment::kNone && moment != g_lastPlace) {
						g_lastPlace = moment;
						Moment_(partner, moment);
					}
					if (moment == Moment::kHomeTown) {
						Spread(partner, false);  // seen together in her town
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Sleep : public RE::BSTEventSink<RE::TESSleepStopEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent*, RE::BSTEventSource<RE::TESSleepStopEvent>*) override
			{
				Queue([]() {
					if (const auto partner = FollowingPartner()) {
						Moment_(partner, Moment::kRest);
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Lover's Comfort is cast more than once on waking (measured): take it away each time while the spouse is hurt
		class MagicApply : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
			{
				const auto& f = Forms::Get();
				if (!a_event || !a_event->target || !a_event->target->IsPlayerRef() || !f.loversComfort) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const bool comfort = std::ranges::any_of(f.loversComfort->effects, [&](const RE::Effect* a_effect) {
					return a_effect && a_effect->baseEffect && a_effect->baseEffect->GetFormID() == a_event->magicEffect;
				});
				if (!comfort) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([]() {
					const auto spouse = Romance::Partner();
					if (!spouse || Romance::GetStage(spouse) != Stage::kMarried) {
						return;
					}
					const auto& s = State::Of(spouse->GetFormID());
					if (s.hurt || s.silentUntilDay > State::Today()) {
						RE::ActorHandle caster{};
						RE::PlayerCharacter::GetSingleton()->AsMagicTarget()->DispelEffect(Forms::Get().loversComfort, caster, nullptr);
						logger::info("Events: no Lover's Comfort while your spouse is hurt");
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register()
	{
		static Crosshair      crosshair;
		static Menus          menus;
		static TopicInfo      topicInfo;
		static Container      container;
		static Bleedout       bleedout;
		static Hit            hit;
		static Combat         combat;
		static Death          death;
		static Cleared        cleared;
		static LocationChange location;
		static Sleep          sleep;
		static MagicApply     magic;
		const auto            holder = RE::ScriptEventSourceHolder::GetSingleton();
		SKSE::GetCrosshairRefEventSource()->AddEventSink(&crosshair);
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menus);
		holder->AddEventSink<RE::TESTopicInfoEvent>(&topicInfo);
		holder->AddEventSink<RE::TESContainerChangedEvent>(&container);
		holder->AddEventSink<RE::TESEnterBleedoutEvent>(&bleedout);
		holder->AddEventSink<RE::TESHitEvent>(&hit);
		holder->AddEventSink<RE::TESCombatEvent>(&combat);
		holder->AddEventSink<RE::TESDeathEvent>(&death);
		holder->AddEventSink<RE::TESActorLocationChangeEvent>(&location);
		holder->AddEventSink<RE::TESSleepStopEvent>(&sleep);
		holder->AddEventSink<RE::TESMagicEffectApplyEvent>(&magic);
		if (const auto source = RE::LocationCleared::GetEventSource()) {
			source->AddEventSink(&cleared);
		}
		logger::info("Events: sinks registered");
	}

	void OnGameLoaded()
	{
		g_down.clear();
		g_lastHit.clear();
		g_lastPlace = Moment::kNone;
	}
}
