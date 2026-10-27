#include "Events.h"

#include "Expose.h"
#include "Family.h"

#include "Children.h"
#include "Confess.h"
#include "Crimes.h"
#include "FirstDate.h"

#include "Candidates.h"
#include "Follower.h"
#include "Forms.h"
#include "Gifts.h"
#include "Replies.h"
#include "Romance.h"
#include "Rules.h"
#include "Scenes.h"
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
			for (const auto partner : Romance::Partners()) {
				if (partner->IsPlayerTeammate()) {
					return partner;  // with two following, the first speaks for the road
				}
			}
			return nullptr;
		}

		// A partner's hello in her lovers' sets (not a hurt, ex or friend one)
		bool IsLoversGreeting(const std::string& a_key)
		{
			for (const auto set : { "dating"sv, "committed"sv, "engaged"sv, "spouse"sv, "love"sv, "marry"sv, "maramal"sv }) {
				const auto prefix = std::format("hello.{}", set);
				if (a_key == prefix || a_key.starts_with(prefix + ".")) {
					return true;
				}
			}
			return false;
		}

		// A partner greets you as hers ("hello.dating", "hello.spouse", ...) where another partner who doesn't know
		// about her hears it (same place, within exposeDistance, awake): it comes out, and both catch you on the spot, once the greeting is over (user,
		// 2026-10-26). In a talk the talk closes first: a line said while the dialogue menu is open never plays.
		RE::ActorHandle              g_greeter{};
		std::vector<RE::ActorHandle> g_overheard{};
		bool                         g_greeterGoodbye{ false };  // her goodbye is playing (closing the talk starts it)
		bool                         g_afterGoodbye{ false };    // the scene waits for that goodbye to end

		void OnGreeting(RE::Actor* a_greeter)
		{
			g_greeter = {};
			g_overheard.clear();
			if (!Romance::IsPartner(a_greeter)) {
				return;
			}
			// she hears it: in the same place, close by, awake (detection isn't needed to hear a greeting)
			const auto player = RE::PlayerCharacter::GetSingleton();
			for (const auto other : Romance::Partners()) {
				if (other == a_greeter || Romance::Knows(other, a_greeter) || !other->Is3DLoaded() || !World::SamePlace(other, player)) {
					continue;
				}
				const bool asleep = other->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kIsSleeping;
				if (!asleep && other->GetPosition().GetDistance(player->GetPosition()) < Rules::Get().exposeDistance) {
					g_overheard.push_back(other->GetHandle());
				}
			}
			if (!g_overheard.empty()) {
				g_greeter = a_greeter->GetHandle();
				logger::info("Events: {} greets you as hers in front of another partner", a_greeter->GetDisplayFullName());
			}
		}

		// the greeting is over: both catch you (the one who overheard first)
		void Exposed(float a_delay)
		{
			const auto greeter = g_greeter.get();
			const auto overheard = std::move(g_overheard);
			g_greeter = {};
			g_overheard.clear();
			g_greeterGoodbye = false;
			g_afterGoodbye = false;
			if (!greeter) {
				return;
			}
			if (!overheard.empty()) {
				const auto other = overheard.front().get();
				if (other && Romance::IsPartner(greeter.get()) && Romance::IsPartner(other.get()) && Expose::Start(greeter.get(), other.get())) {
					return;  // they have it out (others who heard it learn of it there)
				}
			}
			float       delay = a_delay;
			RE::Actor*  first = nullptr;
			for (const auto& handle : overheard) {
				if (const auto other = handle.get(); other && Romance::IsPartner(other.get())) {
					Romance::Caught(other.get(), "heard another partner greet you", delay, greeter.get());
					delay += Rules::Get().caughtLineGap;
					first = first ? first : other.get();
				}
			}
			if (first && Romance::IsPartner(greeter.get())) {
				Romance::Caught(greeter.get(), "your other partner was there", delay, first);
			}
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

		void Moment_(RE::Actor* a_speaker, Moment a_moment, float a_delay = 0.0f)
		{
			const auto now = Clock::now();
			if (!Rules::Get().roadLines || now - g_lastMoment < std::chrono::duration<float>(Rules::Get().momentCooldownSeconds)) {
				return;
			}
			g_lastMoment = now;
			World::SayMoment(a_speaker, a_moment, a_delay);
		}

		// Travelling with another woman where your partner notices you and sees her with her own eyes: a jealous line and a small cost, once a game
		// day (not straying; the session forgets the day on load, so a reload can bring it once more)
		std::unordered_map<RE::FormID, float> g_jealousDay;

		// Someone following you that she'd be jealous of: another candidate, or any woman who isn't one of the old
		// people (ElderRace, ElderRaceVampire)
		bool Rival(RE::Actor* a_actor, RE::Actor* a_partner)
		{
			if (!a_actor || a_actor == a_partner || a_actor->IsPlayerRef() || a_actor->IsDead() || !a_actor->IsPlayerTeammate()) {
				return false;
			}
			if (Candidates::Find(a_actor)) {
				return true;
			}
			const auto base = a_actor->GetActorBase();
			const auto race = a_actor->GetRace();
			if (!base || !race || !base->IsFemale() || !race->HasKeywordString("ActorTypeNPC")) {
				return false;
			}
			const auto id = race->GetFormID();
			return id != 0x00067CD8 && id != 0x000A82BA;
		}

		// What kind of woman she's jealous of: 1 a fighter, 2 not. Only a candidate's add-on says she isn't one
		// ("fighter": false); everyone else is, as a vanilla follower is.
		int RivalKind(RE::Actor* a_actor)
		{
			const auto candidate = Candidates::Find(a_actor);
			return candidate && !candidate->attributes.fighter ? 2 : 1;
		}

		// Returns what happened, for the debug tools: "" when she got jealous
		std::string CheckJealousImpl(RE::Actor* a_partner, bool a_test, std::string* a_kind = nullptr)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!Rules::Get().jealousy) {
				return "jealousy is off (Rules.json)";
			}
			if (!a_partner || a_partner->IsDead()) {
				return "no partner";
			}
			const auto today = std::floor(State::Today());
			if (const auto it = g_jealousDay.find(a_partner->GetFormID()); !a_test && it != g_jealousDay.end() && it->second == today) {
				return "already jealous today";
			}
			const auto distance = Rules::Get().jealousDistance;
			if (!World::Sees(a_partner, player, distance)) {
				return "she hasn't noticed you";
			}
			const auto lists = RE::ProcessLists::GetSingleton();
			if (!lists) {
				return "no actors";
			}
			RE::Actor* other = nullptr;
			RE::Actor* unseen = nullptr;
			for (const auto& handle : lists->highActorHandles) {  // skse-lint: ok only when you meet her or change location
				const auto a = handle.get();
				if (!a || !Rival(a.get(), a_partner) || a->GetPosition().GetDistance(player->GetPosition()) >= distance) {
					continue;
				}
				if (World::InSight(a_partner, a.get(), distance)) {
					other = a.get();
					break;
				}
				unseen = a.get();
			}
			if (!other) {
				if (unseen) {
					logger::info("Events: {} is with you, out of {}'s sight", unseen->GetDisplayFullName(), a_partner->GetDisplayFullName());
					return std::format("{} is with you, out of her sight", unseen->GetDisplayFullName());
				}
				return "no woman following you nearby";
			}
			g_jealousDay[a_partner->GetFormID()] = today;
			const auto kind = RivalKind(other);
			logger::info("Events: {} saw you travelling with {} ({})", a_partner->GetDisplayFullName(), other->GetDisplayFullName(),
				kind == 1 ? "a fighter" : "not a fighter");
			if (a_kind) {
				*a_kind = std::format("{}, {}", other->GetDisplayFullName(), kind == 1 ? "a fighter" : "not a fighter");
			}
			// before her line: its version can depend on it (and her mood lines that day)
			a_partner->AddToFaction(Forms::Get().jealousOf, static_cast<std::int8_t>(kind));
			World::SayMoment(a_partner, Moment::kJealous);
			// jealous for the rest of that day (jealousDays 1), or longer
			State::Of(a_partner->GetFormID()).jealousUntilDay = today + static_cast<float>(Rules::Get().jealousDays);
			Romance::ModApproval(a_partner, -Rules::Get().jealousCost, "you travel with someone else");  // syncs her mood too
			return "";
		}

		void CheckJealous(RE::Actor* a_partner)
		{
			CheckJealousImpl(a_partner, false);
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

		// What a line does when it has been said
		void OnLineSaid(RE::Actor* a_speaker, const std::string& a_key)
		{
			const auto& r = Rules::Get();
			const auto& f = Forms::Get();
			const auto  today = State::Today();
			const auto  candidate = Candidates::Find(a_speaker);
			if (a_key == "stayhome") {
				Children::OnStayHomeSaid(a_speaker);
				return;
			}
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

			if (a_key.starts_with("know.")) {
				// a question: one you hadn't asked counts, one a day (the menu doesn't hide the option until the talk
				// ends, so a second new one the same day counts tomorrow); asking again does nothing
				const int  n = std::clamp(std::atoi(a_key.c_str() + 5), 1, 16) - 1;
				const auto bit = static_cast<std::uint16_t>(1u << n);
				if (!(s.knowAsked & bit) && today - s.lastKnowDay >= 1.0f) {
					s.knowAsked |= bit;
					s.lastKnowDay = today;
					Romance::ModApproval(a_speaker, r.knowGain, "getting to know them");  // only talk: never courting or straying
				}
			} else if (a_key == "askdate.yes" && stage == Stage::kFriend) {
				FirstDate::Start(a_speaker);  // tonight at her spot; the date makes her Dating
			} else if (a_key == "follow.yes") {
				Follower::Recruit(a_speaker);
			} else if (a_key == "kiss.yes") {
				World::Kiss(a_speaker);
				if (today - s.lastKissDay >= 1.0f) {
					s.lastKissDay = today;
					Romance::ModApproval(a_speaker, r.kissGain, "a kiss");
				}
				Romance::Contact(a_speaker);
				if (Romance::IsPartner(a_speaker)) {
					Spread(a_speaker, true);  // a kiss in public spreads even a secret
				}
				Romance::Strayed(a_speaker, "a kiss with someone else");  // from every other partner
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
			} else if (a_key == "plead.yes" && stage == Stage::kEx) {
				s.pleadsLeft = 0;
				a_speaker->AddToFaction(f.approval, static_cast<std::int8_t>(r.winBackApproval));
				Romance::SetStage(a_speaker, Stage::kDating, "took them back");
				Romance::Notify(a_speaker, Event::kWonBack);
			} else if (a_key == "plead.no" && stage == Stage::kEx) {
				s.pleadsLeft = std::max(0, s.pleadsLeft - 1);
				s.nextPleadDay = today + static_cast<float>(candidate->attributes.pleadEvery);
				logger::info("Events: you turned {} down ({} more time(s) she'll ask)", a_speaker->GetDisplayFullName(), s.pleadsLeft);
				Romance::Sync(a_speaker);
			} else if (a_key == "secret.yes") {
				s.secret = true;
			} else if (a_key == "date" && today - s.lastDateDay >= 1.0f) {
				s.lastDateDay = today;
				Romance::Strayed(a_speaker, "a date with someone else");  // from every other partner
				Romance::ModApproval(a_speaker, r.dateGain, "an evening together");
				if (Romance::IsPartner(a_speaker)) {
					Spread(a_speaker, false);
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
			} else if (a_key.starts_with("expose.reply.")) {
				Expose::OnAnswered(a_speaker, a_key);
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
							Children::Count();
							Expose::Check();
							Family::Check();  // at home: she and a child may talk
							Children::Sync(a.get());
							Romance::Sync(a.get(), meeting);
							if (meeting && Romance::IsPartner(a.get())) {
								CheckJealous(a.get());
							}
							if (Romance::IsPartner(a.get())) {
								Confess::Check();
							}
							FirstDate::Check();
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// a refusal line waits for the dialogue menu to close and 1.5 s more (a line said while it is open, or up to
		// about a second after it closes, never plays; measured 2026-10-25)
		RE::ActorHandle g_refusing{};
		RE::ActorHandle g_talking{};  // whoever the open talk is with
		RE::ActorHandle g_talkingChild{};  // an adopted child you're talking to

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
				if (a_event && !a_event->opening && a_event->menuName == RE::DialogueMenu::MENU_NAME && g_talkingChild) {
					Queue([handle = g_talkingChild]() {
						if (const auto a = handle.get()) {
							Children::OnTalked(a.get());
						}
					});
					g_talkingChild = {};
				}
				if (a_event && !a_event->opening && a_event->menuName == RE::DialogueMenu::MENU_NAME) {
					Queue([handle = g_talking]() {
						if (g_greeter) {
							// closing the talk makes her say goodbye, and that line's start is queued just after this
							// task: decide once it has run (a single follow-up task, not a loop)
							Queue([]() {
								if (g_greeterGoodbye) {
									g_afterGoodbye = true;  // the scene comes after her goodbye
								} else {
									Exposed(1.5f);  // a line within ~1 s of the close never plays
								}
							});
						}
						Expose::OnDialogueClosed();
						Family::OnDialogueClosed();
						Confess::OnDialogueClosed();
						FirstDate::OnDialogueClosed();
						Replies::OnDialogueClosed();
						FirstDate::Check();
						if (const auto a = handle.get()) {
							Romance::Sync(a.get());  // she may have started or stopped following you
						}
						Family::Check();  // at home: she and a child may talk now
					});
					g_talking = {};
				}
				if (a_event && !a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME) {
					Queue([]() {
						Children::OnLoadingDone();
						Family::Check();
						Confess::Check();
						FirstDate::Check();
					});
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
					g_talking = speaker->GetHandle();
					if (Children::IsChild(speaker)) {
						g_talkingChild = speaker->GetHandle();
					}
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
				if (a_event && a_event->type == RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd && a_event->speakerRef) {
					const auto info = RE::TESForm::LookupByID<RE::TESTopicInfo>(a_event->topicInfoFormID);
					const auto child = a_event->speakerRef->As<RE::Actor>();
					if (info && info->parentTopic == Forms::Get().childHello && Children::IsChild(child) && child->GetFactionRank(Forms::Get().child, false) > 0) {
						Queue([handle = child->GetHandle()]() {
							if (const auto a = handle.get()) {
								Children::OnGreeted(a.get());
							}
						});
						return RE::BSEventNotifyControl::kContinue;
					}
				}
				if (!a_event || !Candidates::FindLine(a_event->topicInfoFormID, &key)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				logger::info("Events: line '{}' {} ({:08X})", key, a_event->type == RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd ? "ended" : "began",
					a_event->topicInfoFormID);
				Queue([handle = a_event->speakerRef ? a_event->speakerRef->GetHandle() : RE::ObjectRefHandle{}, key,
						  began = a_event->type != RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd]() {
					if (Expose::Running()) {
						const auto ref = handle.get();
						if (began && ref && ref->As<RE::Actor>()) {
							Expose::OnLineBegan(ref->As<RE::Actor>(), key);
						}
						Expose::Check();  // the lines around are its clock: a beat that never started is said again
					}
					if (Family::Running()) {
						const auto ref = handle.get();
						if (began && ref && ref->As<RE::Actor>()) {
							Family::OnLineBegan(ref->As<RE::Actor>(), key);
						}
						Family::Tick();
					}
				});
				if (a_event->type != RE::TESTopicInfoEvent::TopicInfoEventType::kTopicEnd) {
					if (key == "firstdate" && a_event->speakerRef) {
						Queue([handle = a_event->speakerRef->GetHandle()]() {
							if (const auto ref = handle.get(); ref && ref->As<RE::Actor>()) {
								FirstDate::OnSaid(ref->As<RE::Actor>());
							}
						});
					}
					if (key == "confess" && a_event->speakerRef) {
						Queue([handle = a_event->speakerRef->GetHandle()]() {
							if (const auto ref = handle.get(); ref && ref->As<RE::Actor>()) {
								Confess::OnSaid(ref->As<RE::Actor>());
							}
						});
					}
					if (key.starts_with("goodbye.") && a_event->speakerRef) {
						Queue([handle = a_event->speakerRef->GetHandle()]() {
							if (g_greeter && handle.get() == g_greeter.get()) {
								g_greeterGoodbye = true;
							}
						});
					}
					if (IsLoversGreeting(key) && a_event->speakerRef) {
						Queue([handle = a_event->speakerRef->GetHandle()]() {
							if (const auto ref = handle.get(); ref && ref->As<RE::Actor>()) {
								OnGreeting(ref->As<RE::Actor>());
							}
						});
					}
					if (key == "stayhome" && a_event->speakerRef) {  // said once: a second force greet finds nothing to say
						Queue([handle = a_event->speakerRef->GetHandle()]() {
							if (const auto ref = handle.get(); ref && ref->As<RE::Actor>()) {
								ref->As<RE::Actor>()->AddToFaction(Forms::Get().moment, 0);
							}
						});
					}
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto speaker = a_event->speakerRef ? a_event->speakerRef->As<RE::Actor>() : nullptr;
				if (speaker && key.starts_with("family.")) {
					Queue([handle = speaker->GetHandle(), key]() {
						if (const auto a = handle.get()) {
							Family::OnLineEnded(a.get(), key);
						}
					});
				}
				if (speaker && key.starts_with("expose.")) {
					Queue([handle = speaker->GetHandle(), key]() {
						if (const auto a = handle.get()) {
							Expose::OnLineEnded(a.get(), key);
						}
					});
				}
				if (speaker && key.starts_with("goodbye.")) {
					Queue([handle = speaker->GetHandle()]() {
						if (g_greeter && handle.get() == g_greeter.get()) {
							g_greeterGoodbye = false;
							if (g_afterGoodbye) {
								Exposed(0.5f);
							}
						}
					});
				}
				if (speaker && IsLoversGreeting(key)) {
					Queue([handle = speaker->GetHandle()]() {
						if (handle.get() != g_greeter.get() || !g_greeter) {
							return;
						}
						if (RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
							// the talk closes, and the scene plays then (the menu's close runs Exposed)
							RE::UIMessageQueue::GetSingleton()->AddMessage(RE::DialogueMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
						} else {
							Exposed(0.5f);  // a greeting in passing
						}
					});
				}
				if (speaker) {
					RE::FormID candidate = 0;
					const int  reply = Candidates::ReplyTo(a_event->topicInfoFormID, &candidate);
					Queue([handle = speaker->GetHandle(), key, candidate, reply]() {
						if (const auto a = handle.get()) {
							OnLineSaid(a.get(), key);
							if (reply > 0) {
								Replies::OnLineEnded(a.get(), candidate, reply);  // she answers
							}
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
				if (!a_event || a_event->itemCount <= 0) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->newContainer == 0x14) {
					Queue([from = a_event->oldContainer, item = a_event->baseObj, count = a_event->itemCount]() {
						Crimes::OnItemTaken(from, item, count);
					});
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->oldContainer != 0x14) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([item = a_event->baseObj]() { Crimes::OnItemGiven(item); });
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// bleedouts: a following partner who went down this fight (each one costs her bleedout tolerance, nothing
		// comes back for a rescue: user, 2026-10-25)
		std::set<RE::FormID> g_down;

		// The fight is over: back on her feet, she says so (instead of the after-fight line). True if she was down
		bool CheckRevive(RE::Actor* a_actor)
		{
			if (!g_down.contains(a_actor->GetFormID()) || a_actor->AsActorState()->IsBleedingOut()) {
				return false;
			}
			g_down.erase(a_actor->GetFormID());
			if (!a_actor->IsDead()) {
				World::SayMoment(a_actor, Moment::kRevive);
			}
			return true;
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
					g_down.insert(a->GetFormID());
					if (Rules::Get().bleedouts) {
						Romance::Hurt(a.get(), candidate->attributes.bleedout, Why::kBleedouts, "went down beside you");
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// hits on a partner: stray ones in a fight, or an attack
		std::unordered_map<RE::FormID, Clock::time_point> g_lastHit;


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
				if (!a_event->cause->IsPlayerRef() || !IsCandidateAt(target, Stage::kFriend, Stage::kMarried)) {
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
					if (candidate->attributes.waryStyle == Candidates::WaryStyle::kAngry && s.waryUntilDay > State::Today()) {
						Attack(a.get(), "a hit while she's still angry");  // no stray hits while she's angry with you
						return;
					}
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
					if (state == RE::ACTOR_COMBAT_STATE::kCombat && atPlayer && IsCandidateAt(a.get(), Stage::kFriend, Stage::kMarried)) {
						Attack(a.get(), "turned on you");
					} else if (state == RE::ACTOR_COMBAT_STATE::kNone) {
						State::Of(a->GetFormID()).strayHits = 0;
						const bool wasDown = CheckRevive(a.get());
						if (!wasDown && a->IsPlayerTeammate() && !RE::PlayerCharacter::GetSingleton()->IsInCombat()) {
							Moment_(a.get(), Moment::kAfterFight);
						}
						Confess::Check();
						FirstDate::Check();
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Anyone but the victim, your followers and your enemies who saw you strike the killing blow
		bool SomeoneSawKill(RE::Actor* a_victim)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			for (const auto witness : World::Witnesses(player, Rules::Get().witnessDistance)) {
				if (witness != a_victim && !witness->IsHostileToActor(player)) {
					return true;
				}
			}
			return false;
		}

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
						const auto stage = Romance::GetStage(d.get());
						if (stage >= Stage::kDating && stage <= Stage::kMarried) {
							const bool seen = k && k->IsPlayerRef() && SomeoneSawKill(d.get());  // unseen, it's a death like any other
							auto&      s = State::Of(d->GetFormID());
							s.who = seen ? Who::kPlayer : Who::kNone;  // also clears an old breakup's after a win-back
							s.why = seen ? Why::kViolence : Why::kNone;
							if (k && k->IsPlayerRef()) {
								logger::info("Death: you killed {}, {}", d->GetDisplayFullName(), seen ? "seen" : "unseen");
							}
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
				if (!a_event || !a_event->actor || !a_event->actor->IsPlayerRef()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([loc = a_event->newLoc]() {
					Children::OnPlayerLocation(loc);  // the wilds have no location at all
					Family::Check();
					Confess::Check();
					FirstDate::Check();
				});
				if (!a_event->newLoc) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Queue([loc = a_event->newLoc]() {
					for (const auto other : Romance::Partners()) {
						CheckJealous(other);
					}
					const auto partner = FollowingPartner();
					if (!partner) {
						return;
					}
					const auto candidate = Candidates::Find(partner);
					Romance::Contact(partner);
					const auto moment = World::MomentForPlace(loc, candidate->community);
					if (moment != Moment::kNone && moment != g_lastPlace) {
						g_lastPlace = moment;
						Moment_(partner, moment, 3.0f);  // once you're both through the door and looking around
					}
					if (moment == Moment::kHomeTown) {
						Spread(partner, false);  // seen together in her town
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class FastTravel : public RE::BSTEventSink<RE::TESFastTravelEndEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESFastTravelEndEvent*, RE::BSTEventSource<RE::TESFastTravelEndEvent>*) override
			{
				Queue([]() { Children::OnFastTravelEnd(); });
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
					Confess::Check();
					FirstDate::Check();
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

	void Attack(RE::Actor* a_actor, std::string_view a_how)
	{
		const auto candidate = Candidates::Find(a_actor);
		auto&      s = State::Of(a_actor->GetFormID());
		if (s.lockUntilDay > State::Today()) {
			return;  // already counted
		}
		s.strayHits = 0;
		s.lockUntilDay = State::Today() + static_cast<float>(candidate->attributes.attackLockDays);
		s.waryUntilDay = State::Today() + static_cast<float>(candidate->attributes.waryDays);
		if (candidate->attributes.waryStyle == Candidates::WaryStyle::kAfraid) {
			s.lockUntilDay = std::max(s.lockUntilDay, s.waryUntilDay);  // she keeps away: no following, no dates
		}
		logger::info("Events: you attacked {} ({})", a_actor->GetDisplayFullName(), a_how);
		World::SayMoment(a_actor, Moment::kAttacked);
		Follower::Dismiss(a_actor);
		auto tolerance = candidate->attributes.hit;
		if (!tolerance.never) {
			tolerance.cost = Rules::Get().attackCost;
		}
		Romance::Hurt(a_actor, tolerance, Why::kViolence, a_how);
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
		static FastTravel     fastTravel;
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
		holder->AddEventSink<RE::TESFastTravelEndEvent>(&fastTravel);
		holder->AddEventSink<RE::TESMagicEffectApplyEvent>(&magic);
		if (const auto source = RE::LocationCleared::GetEventSource()) {
			source->AddEventSink(&cleared);
		}
		logger::info("Events: sinks registered");
	}

	void TestLine(RE::Actor* a_actor, const std::string& a_key)
	{
		if (a_actor) {
			OnLineSaid(a_actor, a_key);
		}
	}

	bool TestJealous(RE::Actor* a_partner, std::string& a_what)
	{
		std::string kind;
		const auto  why = CheckJealousImpl(a_partner, true, &kind);
		a_what = why.empty() ? kind : why;
		return why.empty();
	}

	void OnGameLoaded()
	{
		g_down.clear();
		g_lastHit.clear();
		g_lastPlace = Moment::kNone;
		g_jealousDay.clear();
		g_greeter = {};
		g_overheard.clear();
		g_greeterGoodbye = false;
		g_afterGoodbye = false;
		Scenes::OnGameLoaded();
		Expose::OnGameLoaded();
		Family::OnGameLoaded();
		g_refusing = {};
		g_talkingChild = {};
		Gifts::OnGameLoaded();
		Children::OnGameLoaded();
		Confess::OnGameLoaded();
		FirstDate::OnGameLoaded();
		Replies::OnGameLoaded();
	}
}
