#include "Candidates.h"

#include "Forms.h"
#include "Romance.h"

namespace Candidates
{
	namespace
	{
		std::vector<Candidate>                                     g_all;
		std::unordered_map<RE::FormID, std::size_t>                g_byRef;
		std::unordered_map<RE::FormID, std::pair<std::size_t, std::string>> g_byLine;
		std::vector<std::string>                                   g_rejected;

		constexpr auto kFolder = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Candidates"sv;

		// "Plugin.esp|0x123456" -> the form (the id is the plugin-local part)
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

		// A condition row the line must carry, ANDed (not part of an OR group), on the speaker
		bool HasRow(const RE::TESTopicInfo* a_info, RE::FUNCTION_DATA::FunctionID a_function, const void* a_param, float a_value)
		{
			for (auto item = a_info->objConditions.head; item; item = item->next) {
				const auto& d = item->data;
				if (d.functionData.function.get() != a_function || d.flags.isOR || d.flags.global) {
					continue;
				}
				if (d.object.get() != RE::CONDITIONITEMOBJECT::kSelf || d.flags.opCode != RE::CONDITION_ITEM_DATA::OpCode::kEqualTo) {
					continue;
				}
				if (d.functionData.params[0] == a_param && d.comparisonValue.f == a_value) {
					return true;
				}
			}
			return false;
		}

		// Checks one line; returns why it fails, empty when it is fine
		std::string CheckLine(const RE::TESTopicInfo* a_info, const Candidate& a_candidate, RE::TESTopic* a_topic, RE::TESFaction* a_faction, int a_rank)
		{
			if (a_info->parentTopic != a_topic) {
				return "is not in the framework's topic for it";
			}
			if (!HasRow(a_info, RE::FUNCTION_DATA::FunctionID::kGetIsID, a_candidate.npc, 1.0f)) {
				return "has no GetIsID == 1 condition for this candidate";
			}
			if (!HasRow(a_info, RE::FUNCTION_DATA::FunctionID::kGetFactionRank, a_faction, static_cast<float>(a_rank))) {
				return std::format("has no GetFactionRank {:08X} == {} condition", a_faction->GetFormID(), a_rank);
			}
			return {};
		}

		void LoadOne(const std::filesystem::path& a_path)
		{
			const auto file = a_path.filename().string();
			Candidate  candidate;
			candidate.manifest = file;
			std::vector<std::string> problems;

			nlohmann::json j;
			try {
				std::ifstream in(a_path);
				j = nlohmann::json::parse(in);
			} catch (const std::exception& e) {
				g_rejected.push_back(std::format("{}: not valid JSON ({})", file, e.what()));
				return;
			}

			const auto who = j.value("candidate", nlohmann::json::object());
			candidate.name = who.value("name", file);
			const auto npc = ParseForm(who.value("npc", ""s));
			candidate.npc = npc ? npc->As<RE::TESNPC>() : nullptr;
			const auto ref = ParseForm(who.value("ref", ""s));
			candidate.ref = ref && ref->As<RE::Actor>() ? ref->GetFormID() : 0;
			if (!candidate.npc) {
				problems.push_back("candidate.npc is missing or not an NPC");
			}
			if (!candidate.ref) {
				problems.push_back("candidate.ref is missing or not a placed actor");
			}
			candidate.attributes = j.value("attributes", nlohmann::json::object());

			const auto lines = j.value("lines", nlohmann::json::object());
			if (candidate.npc) {
				const auto& f = Forms::Get();
				auto require = [&](const std::string& a_key, RE::TESTopic* a_topic, RE::TESFaction* a_faction, int a_rank) {
					const auto list = lines.value(a_key, nlohmann::json::array());
					if (!list.is_array() || list.empty()) {
						problems.push_back(std::format("no '{}' line", a_key));
						return;
					}
					for (const auto& entry : list) {
						const auto text = entry.is_string() ? entry.get<std::string>() : ""s;
						const auto form = ParseForm(text);
						const auto info = form ? form->As<RE::TESTopicInfo>() : nullptr;
						if (!info) {
							problems.push_back(std::format("'{}' line {} is not a dialogue line", a_key, text));
							continue;
						}
						if (auto why = CheckLine(info, candidate, a_topic, a_faction, a_rank); !why.empty()) {
							problems.push_back(std::format("'{}' line {} {}", a_key, text, why));
							continue;
						}
						candidate.lines[a_key].push_back(info);
					}
				};
				require("court", f.court, f.stage, static_cast<int>(Romance::Stage::kFriend));
				for (const auto& [stage, name] : Romance::AmuletStages()) {
					require("amulet."s + name, f.amuletTopic, f.amulet, Romance::AmuletRank(stage));
				}
				for (const auto& [state, name] : Romance::GreetStates()) {
					require("hello."s + name, f.hello, f.greet, static_cast<int>(state));
					require("goodbye."s + name, f.goodbye, f.greet, static_cast<int>(state));
				}
			}

			if (!problems.empty()) {
				std::string all;
				for (const auto& p : problems) {
					all += "\n    - " + p;
				}
				g_rejected.push_back(std::format("{} ({}): not registered{}", candidate.name, file, all));
				return;
			}

			const auto index = g_all.size();
			g_byRef[candidate.ref] = index;
			for (const auto& [key, infos] : candidate.lines) {
				for (const auto info : infos) {
					g_byLine[info->GetFormID()] = { index, key };
				}
			}
			logger::info("Candidates: registered {} ({}), {} line keys", candidate.name, file, candidate.lines.size());
			g_all.push_back(std::move(candidate));
		}
	}

	void Load()
	{
		g_all.clear();
		g_byRef.clear();
		g_byLine.clear();
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
		for (const auto& path : files) {
			LoadOne(path);
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

	const Candidate* Find(const RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}
		const auto it = g_byRef.find(a_actor->GetFormID());
		return it != g_byRef.end() ? &g_all[it->second] : nullptr;
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

	const std::vector<std::string>& Rejected()
	{
		return g_rejected;
	}
}
