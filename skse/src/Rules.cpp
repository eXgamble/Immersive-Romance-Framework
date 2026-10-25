#include "Rules.h"

namespace Rules
{
	namespace
	{
		Values      g_values;
		std::string g_interest = "any";

		constexpr auto kPath = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Rules.json"sv;

		std::set<std::string> g_known;  // the keys Load reads, to flag a misspelt one

		template <class T>
		void Read(const nlohmann::json& a_json, const char* a_key, T& a_value)
		{
			g_known.insert(a_key);
			if (a_json.contains(a_key) && !a_json[a_key].is_number()) {
				logger::warn("Rules: \"{}\" must be a number, keeping {}", a_key, a_value);
			} else if (a_json.contains(a_key)) {
				a_value = a_json[a_key].get<T>();
			}
		}
	}

	void Load()
	{
		g_values = {};
		std::error_code ec;
		if (!std::filesystem::exists(kPath, ec)) {
			logger::info("Rules: no {}, using the defaults", kPath);
			return;
		}
		nlohmann::json j;
		try {
			std::ifstream in{ std::filesystem::path(kPath) };
			j = nlohmann::json::parse(in, nullptr, true, true);
		} catch (const std::exception& e) {
			logger::error("Rules: {} is not valid JSON ({}), using the defaults", kPath, e.what());
			return;
		}
		auto& v = g_values;
#define IRF_RULE(name) Read(j, #name, v.name)
		IRF_RULE(startApproval);
		IRF_RULE(askDateApproval);
		IRF_RULE(commitBar);
		IRF_RULE(floorDating);
		IRF_RULE(talkGain);
		IRF_RULE(dateGain);
		IRF_RULE(kissGain);
		IRF_RULE(giftGain);
		IRF_RULE(giftLikedGain);
		IRF_RULE(giftLovedGain);
		IRF_RULE(giftDislikedCost);
		IRF_RULE(giftInsultCost);
		IRF_RULE(killGain);
		IRF_RULE(killCapPerDay);
		IRF_RULE(clearGain);
		IRF_RULE(bleedoutCost);
		IRF_RULE(reviveBack);
		IRF_RULE(reviveSeconds);
		IRF_RULE(cheatCost);
		IRF_RULE(brokenHeartDays);
		IRF_RULE(hitCost);
		IRF_RULE(strayHitsToAttack);
		IRF_RULE(attackCost);
		IRF_RULE(attackLockDays);
		IRF_RULE(absenceDays);
		IRF_RULE(absenceCost);
		IRF_RULE(jealousCost);
		IRF_RULE(crimeScale);
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
		IRF_RULE(momentCooldownSeconds);
		IRF_RULE(witnessDistance);
		IRF_RULE(caughtDistance);
		IRF_RULE(jealousDistance);
#undef IRF_RULE
		g_known.insert("playerInterestedIn");
		if (j.contains("playerInterestedIn") && j["playerInterestedIn"].is_string()) {
			g_interest = j["playerInterestedIn"].get<std::string>();
		}
		for (const auto& [key, value] : j.items()) {
			if (!g_known.contains(key)) {
				logger::warn("Rules: unknown key \"{}\" (misspelt?), ignored", key);
			}
		}
		logger::info("Rules: loaded {}", kPath);
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
