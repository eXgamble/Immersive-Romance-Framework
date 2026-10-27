#include "Rules.h"

namespace Rules
{
	namespace
	{
		Values      g_values;
		std::string g_interest = "any";

		constexpr auto kPath = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Rules.json"sv;
		constexpr auto kUserPath = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Rules.user.json"sv;

		std::set<std::string> g_known;  // the keys Load reads, to flag a misspelt one

		template <class T>
		void Read(const nlohmann::json& a_json, const char* a_key, T& a_value)
		{
			g_known.insert(a_key);
			if (!a_json.contains(a_key)) {
				return;
			}
			if constexpr (std::is_same_v<T, bool>) {
				if (a_json[a_key].is_boolean()) {
					a_value = a_json[a_key].get<bool>();
				} else {
					logger::warn("Rules: \"{}\" must be true or false, keeping {}", a_key, a_value);
				}
			} else if (a_json[a_key].is_number()) {
				a_value = a_json[a_key].get<T>();
			} else {
				logger::warn("Rules: \"{}\" must be a number, keeping {}", a_key, a_value);
			}
		}

		// The rules a JSON object gives, over what's set already; a switch may stand in the "systems" section or at
		// the top level
		void Apply(const nlohmann::json& a_root, std::string_view a_from)
		{
			const auto& j = a_root;
			auto&       v = g_values;
			const auto& sys = a_root.contains("systems") && a_root["systems"].is_object() ? a_root["systems"] : a_root;
#define IRF_SWITCH(name)        \
	Read(sys, #name, v.name); \
	Read(j, #name, v.name)
			IRF_SWITCH(jealousy);
			IRF_SWITCH(crimes);
			IRF_SWITCH(murderLeaves);
			IRF_SWITCH(absence);
			IRF_SWITCH(bleedouts);
			IRF_SWITCH(exposeScenes);
			IRF_SWITCH(familyTalks);
			IRF_SWITCH(stayHome);
			IRF_SWITCH(confession);
			IRF_SWITCH(serviceRefusal);
			IRF_SWITCH(roadLines);
#undef IRF_SWITCH
			g_known.insert("systems");
#define IRF_RULE(name) Read(j, #name, v.name)
		IRF_RULE(startApproval);
		IRF_RULE(askDateApproval);
		IRF_RULE(commitBar);
		IRF_RULE(uncommitBar);
		IRF_RULE(floorDating);
		IRF_RULE(knowGain);
		IRF_RULE(dateGain);
		IRF_RULE(dateEveryDays);
		IRF_RULE(eveningHours);
		IRF_RULE(dateFromHour);
		IRF_RULE(dateToHour);
		IRF_RULE(kissGain);
		IRF_RULE(wifeGiftEveryDays);
		IRF_RULE(giftGain);
		IRF_RULE(giftLikedGain);
		IRF_RULE(giftLovedGain);
		IRF_RULE(giftDislikedCost);
		IRF_RULE(giftInsultCost);
		IRF_RULE(giftGoldLiked);
		IRF_RULE(giftGoldLoved);
		IRF_RULE(killGain);
		IRF_RULE(killCapPerDay);
		IRF_RULE(clearGain);
		IRF_RULE(bleedoutCost);
		IRF_RULE(cheatCost);
		IRF_RULE(brokenHeartDays);
		IRF_RULE(hitCost);
		IRF_RULE(strayHitsToAttack);
		IRF_RULE(attackCost);
		IRF_RULE(attackLockDays);
		IRF_RULE(absence1);
		IRF_RULE(absence2);
		IRF_RULE(absence3);
		IRF_RULE(absenceCost);
		IRF_RULE(jealousCost);
		IRF_RULE(waryDays);
		IRF_RULE(jealousDays);
		IRF_RULE(band2);
		IRF_RULE(band3);
		IRF_RULE(band4);
		IRF_RULE(band5);
		IRF_RULE(heartbreakSilentDays);
		IRF_RULE(pleads);
		IRF_RULE(pleadEvery);
		IRF_RULE(crimeScale);
		IRF_RULE(murderLeaveAlignment);
		IRF_RULE(winBackDays);
		IRF_RULE(winBackApproval);
		IRF_RULE(amendsApproval);
		IRF_RULE(reactionDays);
		IRF_RULE(garrulousScale);
		IRF_RULE(partnerXpPerLevel);
		IRF_RULE(partnerXpKill);
		IRF_RULE(partnerXpClear);
		IRF_RULE(partnerHealthPerLevel);
		IRF_RULE(partnerSkillPerLevel);
		IRF_RULE(confessDelayDays);
		IRF_RULE(confessMinDatingDays);
		IRF_RULE(confessDistance);
		IRF_RULE(standUpCost);
		IRF_RULE(firstDateDistance);
		IRF_RULE(childAway1);
		IRF_RULE(childAway2);
		IRF_RULE(childAway3);
		IRF_RULE(momentCooldownSeconds);
		IRF_RULE(witnessDistance);
		IRF_RULE(caughtDistance);
		IRF_RULE(jealousDistance);
		IRF_RULE(exposeDistance);
		IRF_RULE(caughtLineGap);
		IRF_RULE(exposeLeaveDistance);
		IRF_RULE(exposePersuade);
		IRF_RULE(exposeDoubtCost);
		IRF_RULE(disownedSpouseMult);
		IRF_RULE(familyHearDistance);
#undef IRF_RULE
			g_known.insert("playerInterestedIn");
			if (j.contains("playerInterestedIn") && j["playerInterestedIn"].is_string()) {
				g_interest = j["playerInterestedIn"].get<std::string>();
			}
			for (const auto& [key, value] : j.items()) {
				if (!g_known.contains(key)) {
					logger::warn("Rules: unknown key \"{}\" in {} (misspelt?), ignored", key, a_from);
				}
			}
			if (&sys != &j) {
				for (const auto& [key, value] : sys.items()) {
					if (!g_known.contains(key)) {
						logger::warn("Rules: unknown system \"{}\" in {} (misspelt?), ignored", key, a_from);
					}
				}
			}
		}

		bool ApplyFile(std::string_view a_path)
		{
			std::error_code ec;
			if (!std::filesystem::exists(a_path, ec)) {
				return false;
			}
			try {
				std::ifstream in{ std::filesystem::path(a_path) };
				Apply(nlohmann::json::parse(in, nullptr, true, true), a_path);
			} catch (const std::exception& e) {
				logger::error("Rules: {} is not valid JSON ({}), skipped", a_path, e.what());
				return false;
			}
			logger::info("Rules: loaded {}", a_path);
			return true;
		}
	}

	void Load()
	{
		g_values = {};
		g_interest = "any";
		if (!ApplyFile(kPath)) {
			logger::info("Rules: no {}, using the defaults", kPath);
		}
		ApplyFile(kUserPath);  // a modlist's own changes, over the framework's file
		const auto& v = g_values;
		logger::info("Rules: systems jealousy {} crimes {} murderLeaves {} absence {} bleedouts {} exposeScenes {} familyTalks {} "
					 "stayHome {} confession {} serviceRefusal {} roadLines {}",
			v.jealousy, v.crimes, v.murderLeaves, v.absence, v.bleedouts, v.exposeScenes, v.familyTalks, v.stayHome, v.confession,
			v.serviceRefusal, v.roadLines);
	}

	bool Set(std::string_view a_name, std::string_view a_json)
	{
		// Papyrus strings come through the game's case-blind string table: "false" may arrive as "False"
		std::string text(a_json);
		std::string lower = text;
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lower == "true" || lower == "false" || lower == "null") {
			text = lower;
		}
		nlohmann::json value;
		try {
			value = nlohmann::json::parse(text);
		} catch (const std::exception&) {
			return false;
		}
		g_known.clear();
		Apply(nlohmann::json{ { std::string(a_name), value } }, "a debug call");
		const bool known = g_known.contains(std::string(a_name));
		logger::info("Rules: {} set to {} ({})", a_name, a_json, known ? "debug" : "no such rule");
		return known;
	}

	const Values& Get()
	{
		return g_values;
	}

	const std::string& PlayerInterest()
	{
		return g_interest;
	}
}
