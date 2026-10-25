#pragma once

#include "Types.h"

// Add-on candidates: one JSON manifest per candidate in Data/SKSE/Plugins/ImmersiveRomanceFramework/Candidates/.
// Read once at kDataLoaded and checked against the requirements; a candidate that falls short is never
// registered and stays on vanilla's flow (the rest of their bundle still registers).
namespace Candidates
{
	// What one kind of hurt costs; "never" in the JSON = unforgivable (one is enough, no way back)
	struct Tolerance
	{
		int  cost{ 0 };
		bool never{ false };
	};

	enum class Scruples
	{
		kRefuse,   // won't be courted by a taken player
		kStayOut,  // friendly, but neither refuses you nor flirts back
		kCompete,  // courts you anyway
	};

	enum class Interest
	{
		kAny,
		kMale,
		kFemale,
	};

	// How she takes being attacked (wary mood): afraid keeps away (no following or dates), angry takes any further
	// hit as an attack, cold gives your talks and dates nothing; none of them kisses you while wary
	enum class WaryStyle
	{
		kAfraid,
		kAngry,
		kCold,
	};

	struct Combat
	{
		std::string                      role;
		RE::TESCombatStyle*              style{ nullptr };
		float                            confidence{ -1.0f };  // actor value while following; -1 = leave as is
		float                            aggression{ -1.0f };
		std::vector<RE::TESBoundObject*> gear;
		int                              levelCap{ 0 };  // growth cap (their real level); 0 = no growth
	};

	struct Gate
	{
		RE::TESQuest* quest{ nullptr };
		std::uint16_t stage{ 0 };
	};

	// Gifts by how they take them; each entry is an item, a keyword the item carries, or a form list holding it
	struct GiftTaste
	{
		std::vector<RE::TESForm*> insulted;
		std::vector<RE::TESForm*> disliked;
		std::vector<RE::TESForm*> liked;
		std::vector<RE::TESForm*> loved;
	};

	struct Attributes
	{
		Tolerance cheat;
		Tolerance hit;
		int       strayHits{ 0 };
		int       attackLockDays{ 0 };
		int       waryDays{ 0 };
		WaryStyle waryStyle{ WaryStyle::kAfraid };
		Tolerance bleedout;
		Tolerance absence;
		int       absenceDays{ 0 };
		int       brokenHeartDays{ 0 };
		int       modesty{ 1 };  // 0 anywhere, 1 not with others watching, 2 only indoors and alone, 3 only at home
		Scruples  scruples{ Scruples::kStayOut };
		int       garrulous{ 30 };  // 0..100, the chance they pass on what they know
		int       alignment{ 0 };   // -5 evil .. 5 good
		Interest  interestedIn{ Interest::kAny };
		bool      refusesSecret{ false };

		GiftTaste                                gifts;
		Combat                                   combat;
		std::map<IRF::Stage, Gate>               gates;  // a stage that waits for a quest stage
	};

	// Someone in the candidate's circle: reacts to what happens with them, may refuse the player service
	struct CircleMember
	{
		std::string                                            name;
		RE::TESNPC*                                            npc{ nullptr };
		RE::FormID                                             ref{ 0 };
		std::set<IRF::Event>                                   refuseOn;          // events that make them refuse service
		int                                                    refuseDays{ 0 };   // 0 = until amends
		bool                                                   refuseTrade{ true };
		bool                                                   refuseTraining{ true };
	};

	struct Candidate
	{
		std::string                name;
		std::string                manifest;
		RE::TESNPC*                npc{ nullptr };
		RE::FormID                 ref{ 0 };
		RE::BGSLocation*           community{ nullptr };  // their town, from their editor location (or the manifest)
		Attributes                 attributes;
		std::vector<CircleMember>  circle;
		std::set<IRF::Event>       reactsTo;  // events their own optional reaction lines cover
		// line key ("court", "hello.dating", ...) -> the add-on's lines for it
		std::map<std::string, std::vector<RE::TESTopicInfo*>> lines;
	};

	void Load();

	// "Plugin.esp|0x123456" -> the form (the id is the plugin-local part); nullptr when it isn't loaded
	RE::TESForm* ParseForm(const std::string& a_text);

	const std::vector<Candidate>& All();
	const Candidate*              Find(const RE::Actor* a_actor);
	const Candidate*              FindByRef(RE::FormID a_ref);
	RE::Actor*                    ActorOf(const Candidate& a_candidate);

	// The candidate and line key an add-on line belongs to (circle members' lines too); nullptr when it's no listed line
	const Candidate* FindLine(RE::FormID a_info, std::string* a_key);

	// Circle members of every candidate, by their reference
	const std::vector<std::pair<const Candidate*, const CircleMember*>>& CircleOf(RE::FormID a_ref);

	// A candidate's community: the town their home location belongs to
	RE::BGSLocation* TownOf(RE::BGSLocation* a_location);

	// The JSON name of an event ("amends", "brokenHeart", ...)
	std::string EventName(IRF::Event a_event);

	// Manifests that were rejected, with their reasons, for the one notification at the first load
	const std::vector<std::string>& Rejected();
}
