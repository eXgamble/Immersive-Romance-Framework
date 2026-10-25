#pragma once

// Add-on candidates: one JSON manifest per candidate in Data/SKSE/Plugins/ImmersiveRomanceFramework/Candidates/.
// Read once at kDataLoaded and checked against the requirements; a candidate that falls short is never
// registered and stays on vanilla's flow (the rest of their bundle still registers).
namespace Candidates
{
	struct Candidate
	{
		std::string   name;
		std::string   manifest;  // file name, for messages
		RE::TESNPC*   npc{ nullptr };
		RE::FormID    ref{ 0 };  // their placed reference
		nlohmann::json attributes;
		// line key ("court", "hello.dating", ...) -> the add-on's lines for it
		std::map<std::string, std::vector<RE::TESTopicInfo*>> lines;
	};

	void Load();

	const std::vector<Candidate>& All();
	const Candidate*              Find(const RE::Actor* a_actor);

	// The candidate and line key an add-on line belongs to; nullptr when it is no listed line
	const Candidate* FindLine(RE::FormID a_info, std::string* a_key);

	// Manifests that were rejected, with their reasons, for the one notification at the first load
	const std::vector<std::string>& Rejected();
}
