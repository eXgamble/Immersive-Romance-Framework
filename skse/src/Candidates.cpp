#include "Candidates.h"

#include "Forms.h"
#include "Rules.h"

namespace Candidates
{
	using namespace IRF;

	namespace
	{
		std::vector<Candidate>                                              g_all;
		std::unordered_map<RE::FormID, std::size_t>                         g_byRef;
		std::unordered_map<RE::FormID, std::pair<std::size_t, std::string>> g_byLine;
		std::unordered_map<RE::FormID, std::vector<std::pair<const Candidate*, const CircleMember*>>> g_circle;
		std::vector<std::string>                                            g_rejected;

		constexpr auto kFolder = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Candidates"sv;

		RE::TESForm* FormAt(const nlohmann::json& a_json, const char* a_key)
		{
			return a_json.contains(a_key) && a_json[a_key].is_string() ? ParseForm(a_json[a_key].get<std::string>()) : nullptr;
		}

		const std::vector<std::pair<Event, const char*>>& EventNames()
		{
			static const std::vector<std::pair<Event, const char*>> names{
				{ Event::kCourting, "courting" },
				{ Event::kDating, "dating" },
				{ Event::kCommitted, "committed" },
				{ Event::kEngaged, "engaged" },
				{ Event::kMarried, "married" },
				{ Event::kAmends, "amends" },
				{ Event::kPlayerEnded, "playerEnded" },
				{ Event::kTheyEnded, "theyEnded" },
				{ Event::kBrokenHeart, "brokenHeart" },
				{ Event::kSeeingSomeone, "seeingSomeone" },
				{ Event::kWidowed, "widowed" },
				{ Event::kWonBack, "wonBack" },
				{ Event::kKilled, "killed" },
			};
			return names;
		}

		std::optional<Event> EventByName(const std::string& a_name)
		{
			for (const auto& [event, name] : EventNames()) {
				if (a_name == name) {
					return event;
				}
			}
			return std::nullopt;
		}

		using Op = RE::CONDITION_ITEM_DATA::OpCode;

		// One line kind: which topic it must sit in, and the rank row it must carry
		struct Requirement
		{
			std::string     key;
			RE::TESTopic*   topic{ nullptr };
			RE::TESFaction* faction{ nullptr };
			int             rank{ 0 };
			Op              op{ Op::kEqualTo };
			bool            goodbye{ false };  // must carry the Goodbye flag
			bool            required{ true };
			RE::TESFaction* faction2{ nullptr };  // a second row, == rank2 (a mood version: the mood)
			int             rank2{ 0 };
		};

		std::vector<Requirement> Requirements(const Attributes& a_attributes)
		{
			const auto&              f = Forms::Get();
			std::vector<Requirement> list;
			auto add = [&](std::string a_key, RE::TESTopic* a_topic, RE::TESFaction* a_faction, int a_rank, bool a_required = true) {
				list.push_back({ std::move(a_key), a_topic, a_faction, a_rank, Op::kEqualTo, false, a_required });
			};

			add("court", f.court, f.taken, 1);
			add("court.taken", f.court, f.taken, 2);

			const std::pair<GreetState, const char*> states[]{
				{ GreetState::kCourting, "courting" },
				{ GreetState::kDating, "dating" },
				{ GreetState::kCommitted, "committed" },
				{ GreetState::kEngaged, "engaged" },
				{ GreetState::kSpouse, "spouse" },
			};
			// her moods: optional versions of the stage's greetings, goodbyes and idles (hurt: idles only, the broken
			// heart has its own greetings)
			const std::pair<Mood, const char*> moods[]{
				{ Mood::kWarm, "warm" }, { Mood::kStrained, "strained" }, { Mood::kJealous, "jealous" }, { Mood::kHurt, "hurt" }, { Mood::kWary, "wary" }
			};
			auto addMood = [&](std::string a_key, RE::TESTopic* a_topic, RE::TESFaction* a_faction, int a_rank, Mood a_mood) {
				add(std::move(a_key), a_topic, a_faction, a_rank, false);
				list.back().faction2 = f.moodFaction;
				list.back().rank2 = Rank(a_mood);
			};
			const std::pair<Stage, const char*> idleStages[]{
				{ Stage::kCourting, "courting" }, { Stage::kDating, "dating" }, { Stage::kCommitted, "committed" },
				{ Stage::kEngaged, "engaged" }, { Stage::kMarried, "spouse" }
			};
			for (const auto& [state, name] : states) {
				add("hello."s + name, f.hello, f.greet, Rank(state));
				add("goodbye."s + name, f.goodbye, f.greet, Rank(state));
				for (const auto& [mood, moodName] : moods) {
					if (mood != Mood::kHurt) {
						addMood(std::format("hello.{}.{}", name, moodName), f.hello, f.greet, Rank(state), mood);
						addMood(std::format("goodbye.{}.{}", name, moodName), f.goodbye, f.greet, Rank(state), mood);
					}
				}
			}
			// her idle chatter, the whole relationship long (a Friend's is vanilla's)
			for (const auto& [stage, name] : idleStages) {
				add("idle."s + name, f.idle, f.stage, Rank(stage));
				for (const auto& [mood, moodName] : moods) {
					addMood(std::format("idle.{}.{}", name, moodName), f.idle, f.stage, Rank(stage), mood);
				}
			}
			add("idle.ex", f.idle, f.stage, Rank(Stage::kEx));
			add("hello.brokenheart.silent", f.hello, f.greet, Rank(GreetState::kBrokenSilent));
			list.back().goodbye = true;
			add("hello.brokenheart.hurt", f.hello, f.greet, Rank(GreetState::kBrokenHurt));
			add("goodbye.brokenheart", f.goodbye, f.greet, Rank(GreetState::kBrokenHurt));

			const std::pair<Why, const char*> whys[]{
				{ Why::kCheating, "cheating" },
				{ Why::kViolence, "violence" },
				{ Why::kAbsence, "absence" },
				{ Why::kBleedouts, "bleedouts" },
				{ Why::kCrime, "crime" },
				{ Why::kChoice, "choice" },
			};
			for (const auto& [why, name] : whys) {
				add("hello.ex."s + name, f.hello, f.greet, Rank(GreetState::kEx) + Rank(why));
				add("goodbye.ex."s + name, f.goodbye, f.greet, Rank(GreetState::kEx) + Rank(why));
				if (why != Why::kChoice) {
					add("breakup."s + name, f.reactionHello, f.reaction, Rank(Event::kBreakupSpeech) + Rank(why));
				}
			}

			add("amulet.friend", f.amuletTopic, f.amulet, Rank(Stage::kFriend) + 1);
			add("amulet.courting", f.amuletTopic, f.amulet, Rank(Stage::kCourting) + 1);
			add("amulet.dating", f.amuletTopic, f.amulet, Rank(Stage::kDating) + 1);
			for (int band = 1; band <= 5; ++band) {
				add(std::format("howarewe.{}", band), f.howAreWe, f.band, band);
			}
			add("askdate.yes", f.askDateTopic, f.askDate, 1);
			add("askdate.notyet", f.askDateTopic, f.askDate, 2);
			add("follow.yes", f.followTopic, f.follow, 1);
			add("follow.no", f.followTopic, f.follow, 2);
			add("kiss.yes", f.kissTopic, f.kiss, 1);
			add("kiss.nothere", f.kissTopic, f.kiss, 2);
			add("kiss.no", f.kissTopic, f.kiss, 3);
			add("breakup", f.breakUpTopic, f.breakUp, 1);
			add("amends", f.amendsTopic, f.amends, 1);
			add("winback.yes", f.winBackTopic, f.winBack, 1);
			add("winback.no", f.winBackTopic, f.winBack, 2);
			add("secret.yes", f.secretTopic, f.secret, 1);
			add("secret.no", f.secretTopic, f.secret, 2);
			add("talk", f.talkTopic, f.talk, 1);
			add("confront", f.reactionHello, f.reaction, Rank(Event::kConfront));
			add("deny.works", f.denyTopic, f.confront, 1);
			add("deny.fails", f.denyTopic, f.confront, 2);
			add("admit", f.admitTopic, f.confront, 0);
			list.back().op = Op::kGreaterThan;
			for (int band = 1; band <= 3; ++band) {
				add(std::format("absence.{}", band), f.absenceHello, f.absence, band);
			}

			const std::pair<Moment, const char*> moments[]{
				{ Moment::kCaught, "caught" },
				{ Moment::kRevive, "revive" },
				{ Moment::kAttacked, "attacked" },
				{ Moment::kJealous, "jealous" },
				{ Moment::kAfterFight, "road.afterfight" },
				{ Moment::kDragon, "road.dragon" },
				{ Moment::kNearlyDead, "road.nearlydead" },
				{ Moment::kRest, "road.rest" },
				{ Moment::kNordicRuin, "road.nordicruin" },
				{ Moment::kDwarvenRuin, "road.dwarvenruin" },
				{ Moment::kCave, "road.cave" },
				{ Moment::kDungeon, "road.dungeon" },
				{ Moment::kTown, "road.town" },
				{ Moment::kInn, "road.inn" },
				{ Moment::kHomeTown, "road.hometown" },
			};
			for (const auto& [moment, name] : moments) {
				add(name, f.say, f.moment, Rank(moment));
			}
			// with adopted children she won't leave town: her force greet at the edge (the line ends the talk)
			add("stayhome", f.stayHomeTopic, f.moment, Rank(Moment::kStayHome));
			// her answer to a gift, as her goodbye
			add("gift.insulted", f.giftAnswer, f.gift, Rank(Taste::kInsulted) + 1);
			add("gift.disliked", f.giftAnswer, f.gift, Rank(Taste::kDisliked) + 1);
			add("gift", f.giftAnswer, f.gift, Rank(Taste::kNeutral) + 1);
			add("gift.liked", f.giftAnswer, f.gift, Rank(Taste::kLiked) + 1);
			add("gift.loved", f.giftAnswer, f.gift, Rank(Taste::kLoved) + 1);
			add("crime.approve", f.say, f.moment, Rank(Moment::kCrimeApprove), a_attributes.alignment < 0);
			add("crime.disapprove", f.say, f.moment, Rank(Moment::kCrimeDisapprove), a_attributes.alignment > 0);

			// optional: the candidate's own reactions to events, and lines for a while after an event
			for (const auto& [event, name] : EventNames()) {
				add("reaction."s + name, f.reactionHello, f.reaction, Rank(event), false);
				add("recent."s + name, f.recentHello, f.recent, Rank(event), false);
			}
			return list;
		}

		// A condition row the line must carry, ANDed (not part of an OR group), on the speaker
		bool HasRow(const RE::TESTopicInfo* a_info, RE::FUNCTION_DATA::FunctionID a_function, const void* a_param, float a_value, Op a_op = Op::kEqualTo)
		{
			for (auto item = a_info->objConditions.head; item; item = item->next) {
				const auto& d = item->data;
				if (d.functionData.function.get() != a_function || d.flags.isOR || d.flags.global) {
					continue;
				}
				if (d.object.get() != RE::CONDITIONITEMOBJECT::kSelf || d.flags.opCode != a_op) {
					continue;
				}
				if (d.functionData.params[0] == a_param && d.comparisonValue.f == a_value) {
					return true;
				}
			}
			return false;
		}

		// Checks one line; returns why it fails, empty when it is fine
		std::string CheckLine(const RE::TESTopicInfo* a_info, RE::TESNPC* a_speaker, const Requirement& a_req)
		{
			if (a_info->parentTopic != a_req.topic) {
				return "is not in the framework's topic for it";
			}
			if (!HasRow(a_info, RE::FUNCTION_DATA::FunctionID::kGetIsID, a_speaker, 1.0f)) {
				return "has no GetIsID == 1 condition for its speaker";
			}
			if (!HasRow(a_info, RE::FUNCTION_DATA::FunctionID::kGetFactionRank, a_req.faction, static_cast<float>(a_req.rank), a_req.op)) {
				return std::format("has no GetFactionRank {:08X} {} {} condition", a_req.faction->GetFormID(),
					a_req.op == Op::kEqualTo ? "==" : ">", a_req.rank);
			}
			// bit 0 of the INFO flags is Goodbye in the file format; CommonLib v10 names it kStartSceneOnEnd
			if (a_req.faction2 && !HasRow(a_info, RE::FUNCTION_DATA::FunctionID::kGetFactionRank, a_req.faction2, static_cast<float>(a_req.rank2))) {
				return std::format("has no GetFactionRank {:08X} == {} condition", a_req.faction2->GetFormID(), a_req.rank2);
			}
			if (a_req.goodbye && !a_info->data.flags.any(RE::TOPIC_INFO_DATA::TOPIC_INFO_FLAGS::kStartSceneOnEnd)) {
				return "needs the Goodbye flag";
			}
			return {};
		}

		// Reads and checks the lines listed under a_req.key; adds the good ones to a_out[a_storeKey]
		void TakeLines(const nlohmann::json& a_lines, const Requirement& a_req, RE::TESNPC* a_speaker, const std::string& a_storeKey,
			std::map<std::string, std::vector<RE::TESTopicInfo*>>& a_out, std::vector<std::string>& a_problems)
		{
			const auto list = a_lines.value(a_req.key, nlohmann::json::array());
			if (!list.is_array() || list.empty()) {
				if (a_req.required) {
					a_problems.push_back(std::format("no '{}' line", a_req.key));
				}
				return;
			}
			for (const auto& entry : list) {
				const auto text = entry.is_string() ? entry.get<std::string>() : ""s;
				const auto form = ParseForm(text);
				const auto info = form ? form->As<RE::TESTopicInfo>() : nullptr;
				if (!info) {
					a_problems.push_back(std::format("'{}' line {} is not a dialogue line", a_req.key, text));
					continue;
				}
				if (auto why = CheckLine(info, a_speaker, a_req); !why.empty()) {
					a_problems.push_back(std::format("'{}' line {} {}", a_req.key, text, why));
					continue;
				}
				a_out[a_storeKey].push_back(info);
			}
		}

		Tolerance ReadTolerance(const nlohmann::json& a_value, int a_default)
		{
			if (a_value.is_string() && a_value.get<std::string>() == "never") {
				return { 0, true };
			}
			if (a_value.is_number()) {
				return { a_value.get<int>(), false };
			}
			if (a_value.is_object() && a_value.contains("cost") && a_value["cost"].is_number()) {
				return { a_value["cost"].get<int>(), false };
			}
			if (a_value.is_object() && a_value.value("never", false)) {
				return { 0, true };
			}
			return { a_default, false };
		}

		Attributes ReadAttributes(const nlohmann::json& a, std::vector<std::string>& a_problems)
		{
			const auto& r = Rules::Get();
			Attributes  out;
			out.cheat = ReadTolerance(a.value("cheatTolerance", nlohmann::json()), r.cheatCost);
			const auto hit = a.value("hitTolerance", nlohmann::json());
			out.hit = ReadTolerance(hit, r.hitCost);
			out.strayHits = hit.is_object() ? hit.value("strayHits", r.strayHitsToAttack) : r.strayHitsToAttack;
			out.attackLockDays = hit.is_object() ? hit.value("lockDays", r.attackLockDays) : r.attackLockDays;
			const auto wary = a.value("wary", nlohmann::json::object());
			out.waryDays = wary.value("days", r.waryDays);
			const auto style = wary.value("style", "afraid"s);
			out.waryStyle = style == "angry" ? WaryStyle::kAngry : style == "cold" ? WaryStyle::kCold : WaryStyle::kAfraid;
			out.bleedout = ReadTolerance(a.value("bleedoutTolerance", nlohmann::json()), r.bleedoutCost);
			const auto absence = a.value("absenceTolerance", nlohmann::json());
			out.absence = ReadTolerance(absence, r.absenceCost);
			out.absenceDays = absence.is_object() ? absence.value("days", r.absenceDays) : r.absenceDays;
			out.brokenHeartDays = a.value("brokenHeartDays", r.brokenHeartDays);
			out.modesty = std::clamp(a.value("modesty", 1), 0, 3);
			const auto scruples = a.value("scruples", "stayout"s);
			out.scruples = scruples == "refuse" ? Scruples::kRefuse : scruples == "compete" ? Scruples::kCompete : Scruples::kStayOut;
			out.garrulous = std::clamp(a.value("garrulous", 30), 0, 100);
			out.alignment = std::clamp(a.value("alignment", 0), -5, 5);
			const auto interest = a.value("interestedIn", "any"s);
			out.interestedIn = interest == "male" ? Interest::kMale : interest == "female" ? Interest::kFemale : Interest::kAny;
			out.refusesSecret = a.value("refusesSecret", false);

			const auto gifts = a.value("gifts", nlohmann::json::object());
			const std::pair<const char*, std::vector<RE::TESForm*>*> tastes[]{
				{ "insulted", &out.gifts.insulted }, { "disliked", &out.gifts.disliked }, { "liked", &out.gifts.liked }, { "loved", &out.gifts.loved }
			};
			for (const auto& [name, list] : tastes) {
				for (const auto& gift : gifts.value(name, nlohmann::json::array())) {
					const auto form = gift.is_string() ? ParseForm(gift.get<std::string>()) : nullptr;
					if (form && (form->IsBoundObject() || form->Is(RE::FormType::Keyword, RE::FormType::FormList))) {
						list->push_back(form);
					} else {
						a_problems.push_back(std::format("gifts.{}: {} is not a loaded item, keyword or form list", name, gift.dump()));
					}
				}
			}

			const auto combat = a.value("combat", nlohmann::json::object());
			out.combat.role = combat.value("role", ""s);
			if (const auto style = FormAt(combat, "combatStyle")) {
				out.combat.style = style->As<RE::TESCombatStyle>();
			}
			out.combat.confidence = combat.value("confidence", -1.0f);
			out.combat.aggression = combat.value("aggression", -1.0f);
			out.combat.levelCap = combat.value("levelCap", 0);
			for (const auto& item : combat.value("gear", nlohmann::json::array())) {
				const auto form = item.is_string() ? ParseForm(item.get<std::string>()) : nullptr;
				if (const auto bound = form ? form->As<RE::TESBoundObject>() : nullptr) {
					out.combat.gear.push_back(bound);
				} else {
					a_problems.push_back("a combat.gear entry is not an item");
				}
			}

			const std::pair<const char*, Stage> gateStages[]{ { "dating", Stage::kDating }, { "committed", Stage::kCommitted } };
			const auto gates = a.value("gates", nlohmann::json::object());
			for (const auto& [name, stage] : gateStages) {
				if (!gates.contains(name)) {
					continue;
				}
				const auto quest = FormAt(gates[name], "quest");
				if (!quest || !quest->As<RE::TESQuest>()) {
					a_problems.push_back(std::format("gates.{}.quest is not a quest", name));
					continue;
				}
				out.gates[stage] = { quest->As<RE::TESQuest>(), static_cast<std::uint16_t>(gates[name].value("stage", 0)) };
			}
			return out;
		}

		void LoadOne(const std::filesystem::path& a_path)
		{
			const auto               file = a_path.filename().string();
			Candidate                candidate;
			std::vector<std::string> problems;
			candidate.manifest = file;

			nlohmann::json j;
			try {
				std::ifstream in(a_path);
				j = nlohmann::json::parse(in, nullptr, true, true);
			} catch (const std::exception& e) {
				g_rejected.push_back(std::format("{}: not valid JSON ({})", file, e.what()));
				return;
			}

			const auto who = j.value("candidate", nlohmann::json::object());
			candidate.name = who.value("name", file);
			const auto npc = FormAt(who, "npc");
			candidate.npc = npc ? npc->As<RE::TESNPC>() : nullptr;
			const auto ref = FormAt(who, "ref");
			const auto actor = ref ? ref->As<RE::Actor>() : nullptr;
			candidate.ref = actor ? actor->GetFormID() : 0;
			if (!candidate.npc) {
				problems.push_back("candidate.npc is missing or not an NPC");
			}
			if (!actor) {
				problems.push_back("candidate.ref is missing or not a placed actor");
			}
			if (const auto town = FormAt(who, "community")) {
				candidate.community = town->As<RE::BGSLocation>();
			} else if (actor) {
				candidate.community = TownOf(actor->GetEditorLocation1());
			}
			candidate.attributes = ReadAttributes(j.value("attributes", nlohmann::json::object()), problems);

			const auto lines = j.value("lines", nlohmann::json::object());
			if (candidate.npc) {
				for (const auto& req : Requirements(candidate.attributes)) {
					TakeLines(lines, req, candidate.npc, req.key, candidate.lines, problems);
					if (req.key.starts_with("reaction.") && candidate.lines.contains(req.key)) {
						if (const auto event = EventByName(req.key.substr(9))) {
							candidate.reactsTo.insert(*event);
						}
					}
				}
			}

			// the circle: reactions and refusals, spoken by them
			const auto& f = Forms::Get();
			int         index = 0;
			for (const auto& c : j.value("circle", nlohmann::json::array())) {
				CircleMember member;
				member.name = c.value("name", std::format("circle #{}", index));
				const auto cnpc = FormAt(c, "npc");
				member.npc = cnpc ? cnpc->As<RE::TESNPC>() : nullptr;
				const auto cref = FormAt(c, "ref");
				member.ref = cref && cref->As<RE::Actor>() ? cref->GetFormID() : 0;
				if (!member.npc || !member.ref) {
					problems.push_back(std::format("circle member {} needs npc and ref", member.name));
					++index;
					continue;
				}
				const auto reactions = c.value("reactions", nlohmann::json::object());
				for (const auto& [name, list] : reactions.items()) {
					const auto event = EventByName(name);
					if (!event) {
						problems.push_back(std::format("circle member {}: unknown event '{}'", member.name, name));
						continue;
					}
					nlohmann::json wrap{ { name, list } };
					TakeLines(wrap, { name, f.reactionHello, f.reaction, Rank(*event) }, member.npc, std::format("circle.{}.reaction.{}", index, name),
						candidate.lines, problems);
				}
				if (c.contains("refuse")) {
					const auto refuse = c["refuse"];
					for (const auto& name : refuse.value("on", nlohmann::json::array())) {
						if (const auto event = name.is_string() ? EventByName(name.get<std::string>()) : std::nullopt) {
							member.refuseOn.insert(*event);
						}
					}
					const auto until = refuse.value("until", nlohmann::json("amends"));
					member.refuseDays = until.is_number() ? until.get<int>() : 0;
					const auto services = refuse.value("services", nlohmann::json::array({ "trade", "training" }));
					member.refuseTrade = std::ranges::find(services, "trade") != services.end();
					member.refuseTraining = std::ranges::find(services, "training") != services.end();
					nlohmann::json wrap{ { "lines", refuse.value("lines", nlohmann::json::array()) } };
					TakeLines(wrap, { "lines", f.say, f.moment, Rank(Moment::kRefuse) }, member.npc, std::format("circle.{}.refuse", index),
						candidate.lines, problems);
				}
				candidate.circle.push_back(member);
				++index;
			}

			if (!problems.empty()) {
				std::string all;
				for (const auto& p : problems) {
					all += "\n    - " + p;
				}
				g_rejected.push_back(std::format("{} ({}): not registered{}", candidate.name, file, all));
				return;
			}

			const auto i = g_all.size();
			g_byRef[candidate.ref] = i;
			for (const auto& [key, infos] : candidate.lines) {
				for (const auto info : infos) {
					g_byLine[info->GetFormID()] = { i, key };
				}
			}
			logger::info("Candidates: registered {} ({}), {} line keys, {} in their circle, community {}", candidate.name, file, candidate.lines.size(),
				candidate.circle.size(), candidate.community ? candidate.community->GetName() : "none");
			g_all.push_back(std::move(candidate));
		}
	}

	RE::BGSLocation* TownOf(RE::BGSLocation* a_location)
	{
		const auto&      f = Forms::Get();
		RE::BGSLocation* habitation = nullptr;
		for (auto loc = a_location; loc; loc = loc->parentLoc) {
			if (loc->HasKeyword(f.locCity) || loc->HasKeyword(f.locTown) || loc->HasKeyword(f.locSettlement)) {
				return loc;
			}
			if (!habitation && loc->HasKeyword(f.locHabitation)) {
				habitation = loc;
			}
		}
		return habitation;
	}

	RE::TESForm* ParseForm(const std::string& a_text)
	{
		const auto bar = a_text.find('|');
		if (bar == std::string::npos) {
			return nullptr;
		}
		const auto plugin = a_text.substr(0, bar);
		RE::FormID id = 0;
		try {
			id = static_cast<RE::FormID>(std::stoul(a_text.substr(bar + 1), nullptr, 16));
		} catch (...) {
			return nullptr;
		}
		return RE::TESDataHandler::GetSingleton()->LookupForm(id & 0xFFFFFF, plugin);
	}

	void Load()
	{
		g_all.clear();
		g_byRef.clear();
		g_byLine.clear();
		g_circle.clear();
		g_rejected.clear();
		std::error_code ec;
		if (!std::filesystem::exists(kFolder, ec)) {
			logger::info("Candidates: no {} folder, no candidates", kFolder);
			return;
		}
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::directory_iterator(kFolder, ec)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				files.push_back(entry.path());
			}
		}
		std::ranges::sort(files);
		g_all.reserve(files.size());  // the circle index below keeps pointers into g_all
		for (const auto& path : files) {
			LoadOne(path);
		}
		for (const auto& candidate : g_all) {
			for (const auto& member : candidate.circle) {
				g_circle[member.ref].push_back({ &candidate, &member });
			}
		}
		for (const auto& r : g_rejected) {
			logger::warn("Candidates: {}", r);
		}
		logger::info("Candidates: {} registered, {} rejected", g_all.size(), g_rejected.size());
	}

	const std::vector<Candidate>& All()
	{
		return g_all;
	}

	const Candidate* FindByRef(RE::FormID a_ref)
	{
		const auto it = g_byRef.find(a_ref);
		return it != g_byRef.end() ? &g_all[it->second] : nullptr;
	}

	const Candidate* Find(const RE::Actor* a_actor)
	{
		return a_actor ? FindByRef(a_actor->GetFormID()) : nullptr;
	}

	RE::Actor* ActorOf(const Candidate& a_candidate)
	{
		return RE::TESForm::LookupByID<RE::Actor>(a_candidate.ref);
	}

	const Candidate* FindLine(RE::FormID a_info, std::string* a_key)
	{
		const auto it = g_byLine.find(a_info);
		if (it == g_byLine.end()) {
			return nullptr;
		}
		if (a_key) {
			*a_key = it->second.second;
		}
		return &g_all[it->second.first];
	}

	const std::vector<std::pair<const Candidate*, const CircleMember*>>& CircleOf(RE::FormID a_ref)
	{
		static const std::vector<std::pair<const Candidate*, const CircleMember*>> none;
		const auto it = g_circle.find(a_ref);
		return it != g_circle.end() ? it->second : none;
	}

	std::string EventName(Event a_event)
	{
		for (const auto& [event, name] : EventNames()) {
			if (event == a_event) {
				return name;
			}
		}
		return {};
	}

	const std::vector<std::string>& Rejected()
	{
		return g_rejected;
	}
}
