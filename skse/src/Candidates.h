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

	// How she takes it when you end it: accepting (an ex at once), bitter (won't talk to you for a while; also when
	// she catches you), pleading (asks you to take her back when you talk to her, a few times)
	enum class Heartbreak
	{
		kAccepting,
		kBitter,
		kPleading,
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
		Heartbreak heartbreak{ Heartbreak::kAccepting };
		int       silentDays{ 0 };   // bitter
		int       pleads{ 0 };       // pleading
		int       pleadEvery{ 0 };
		Tolerance bleedout;
		Tolerance absence;
		std::array<int, 3> absenceDays{};  // apart longer than each: absence band 1, 2, 3
		int       brokenHeartDays{ 0 };
		float     confessMinDatingDays{ 0.0f };  // her confession waits until you've been Dating this long
		int       modesty{ 1 };  // 0 anywhere, 1 not with others watching, 2 only indoors and alone, 3 only at home
		Scruples  scruples{ Scruples::kStayOut };
		int       garrulous{ 30 };  // 0..100, the chance they pass on what they know
		int       alignment{ 0 };   // -5 evil .. 5 good
		Interest  interestedIn{ Interest::kAny };
		bool      refusesSecret{ false };
		bool      fighter{ true };  // what another partner's jealousy takes her for (any non-candidate is a fighter)
		RE::TESForm* spouseBonus{ nullptr };  // her own spouse perk: an ability (SPEL) or a perk the player has while married
		RE::TESObjectREFR* dateSpot{ nullptr };  // firstDate.spot: where she waits for your first date (required)
		RE::BGSLocation*   datePlace{ nullptr }; // firstDate.place: its name in the journal (default: the spot's cell's)

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
		std::set<IRF::Why>                                     refuseOnBreakup;   // "theyEnded.<why>": she left you for this
		int                                                    refuseDays{ 0 };   // 0 = until amends
		bool                                                   refuseTrade{ true };
		bool                                                   refuseTraining{ true };
	};

	// A family conversation (her add-on's own): her and an adopted child, lines in turn, when home is in this state
	struct FamilyTalk
	{
		std::uint8_t                                    homes{ 0 };   // the home states it plays in (bit 1 << IRF::Home)
		int                                             mood{ -1 };   // her mood (IRF::Mood), -1 any
		RE::FormID                                      alive{ 0 };   // only while this person lives ("alive": a placed actor)
		RE::FormID                                      dead{ 0 };    // only after they died
		int                                             child{ 0 };   // 0 any child, 1 a boy, 2 a girl
		std::vector<std::pair<bool, RE::TESTopicInfo*>> steps;        // (said by the child, the line), in order
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
		std::map<RE::FormID, int>  replyAfter;  // a line (anyone's) -> the number of her reply to it
		std::vector<FamilyTalk>    family;      // her conversations with the children (number = index + 1)
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

	// The number of a candidate's reply to this line (0: none), and whose (her reference)
	int ReplyTo(RE::FormID a_info, RE::FormID* a_candidate);

	// Circle members of every candidate, by their reference
	const std::vector<std::pair<const Candidate*, const CircleMember*>>& CircleOf(RE::FormID a_ref);

	// A candidate's community: the town their home location belongs to
	RE::BGSLocation* TownOf(RE::BGSLocation* a_location);

	// The JSON name of an event ("amends", "brokenHeart", ...)
	std::string EventName(IRF::Event a_event);

	// Manifests that were rejected, with their reasons, for the one notification at the first load
	const std::vector<std::string>& Rejected();
}
