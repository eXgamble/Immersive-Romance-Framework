#pragma once

// The framework's own records (Immersive Romance Framework.esp) and the vanilla ones it touches. FormIDs and
// EditorIDs of the framework's records are frozen from 1.0: add-ons put lines into its topics and their voice
// file paths contain these names.
namespace Forms
{
	inline constexpr auto kPlugin = "Immersive Romance Framework.esp"sv;

	enum Local : RE::FormID
	{
		kCandidateFaction = 0x800,  // every registered candidate, rank 0
		kStageFaction = 0x801,      // rank = Stage; not a member before Friend
		kApprovalFaction = 0x802,   // rank = approval 0..100
		kGreetFaction = 0x803,      // rank = GreetState, what the hellos and goodbyes read
		kQuestBusyFaction = 0x804,  // rank 1 while a running quest needs them (planned)
		kQuest = 0x805,             // IRF_Quest, priority 105, start game enabled
		kHelloTopic = 0x806,
		kGoodbyeTopic = 0x807,
		kCourtTopic = 0x808,     // "I'd like to get to know you better."
		kHowAreWeTopic = 0x80A,  // "How are we?"
	};

	struct Set
	{
		RE::TESFaction* candidate{ nullptr };
		RE::TESFaction* stage{ nullptr };
		RE::TESFaction* approval{ nullptr };
		RE::TESFaction* greet{ nullptr };
		RE::TESFaction* questBusy{ nullptr };
		RE::TESQuest*   quest{ nullptr };
		RE::TESTopic*   hello{ nullptr };
		RE::TESTopic*   goodbye{ nullptr };
		RE::TESTopic*   court{ nullptr };
		RE::TESTopic*   howAreWe{ nullptr };
		RE::TESFaction* potentialMarriage{ nullptr };  // vanilla PotentialMarriageFaction 019809
	};

	// Looks everything up once at kDataLoaded; false when the plugin is missing or a record is
	bool Load();
	const Set& Get();
}
