#include "Gifts.h"

#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "State.h"
#include "World.h"

namespace Gifts
{
	using namespace IRF;

	namespace
	{
		constexpr auto kFolder = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Gifts"sv;

		// the strongest gift of this menu visit, answered once the menus close
		RE::ActorHandle g_pending;
		Taste           g_pendingTaste{ Taste::kNeutral };

		bool Matches(RE::TESForm* a_entry, RE::TESForm* a_item, bool a_outright)
		{
			if (a_outright) {
				return a_entry == a_item;
			}
			if (const auto keyword = a_entry->As<RE::BGSKeyword>()) {
				const auto keywords = a_item->As<RE::BGSKeywordForm>();
				return keywords && keywords->HasKeyword(keyword);
			}
			if (const auto list = a_entry->As<RE::BGSListForm>()) {
				return list->HasForm(a_item);
			}
			return false;
		}

		// how strongly a gift is felt: the answer of a visit is to the strongest one
		int Strength(Taste a_taste)
		{
			switch (a_taste) {
			case Taste::kInsulted:
				return 4;
			case Taste::kLoved:
				return 3;
			case Taste::kDisliked:
				return 2;
			case Taste::kLiked:
				return 1;
			default:
				return 0;
			}
		}
	}

	void Load()
	{
		const auto keyword = Forms::Get().giftInsulting;
		std::error_code ec;
		if (!keyword || !std::filesystem::exists(kFolder, ec)) {
			return;
		}
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::directory_iterator(kFolder, ec)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				files.push_back(entry.path());
			}
		}
		std::ranges::sort(files);
		int tagged = 0;
		for (const auto& path : files) {
			nlohmann::json j;
			try {
				std::ifstream in{ path };
				j = nlohmann::json::parse(in, nullptr, true, true);
			} catch (const std::exception& e) {
				logger::error("Gifts: {} is not valid JSON ({})", path.filename().string(), e.what());
				continue;
			}
			for (const auto& entry : j.value("insulting", nlohmann::json::array())) {
				const auto form = entry.is_string() ? Candidates::ParseForm(entry.get<std::string>()) : nullptr;
				const auto keywords = form ? form->As<RE::BGSKeywordForm>() : nullptr;
				if (!keywords) {
					logger::info("Gifts: {}: {} is not a loaded item, skipped", path.filename().string(), entry.dump());
					continue;
				}
				if (!keywords->HasKeyword(keyword)) {
					keywords->AddKeyword(keyword);
					++tagged;
				}
			}
		}
		logger::info("Gifts: {} items tagged as insulting", tagged);
	}

	Taste TasteOf(const Candidates::Candidate& a_candidate, RE::TESForm* a_item)
	{
		const auto& g = a_candidate.attributes.gifts;
		// the strongest objection first, so a keyword they like never outweighs one they hate
		const std::pair<Taste, const std::vector<RE::TESForm*>*> lists[]{
			{ Taste::kInsulted, &g.insulted }, { Taste::kDisliked, &g.disliked }, { Taste::kLoved, &g.loved }, { Taste::kLiked, &g.liked }
		};
		for (const bool outright : { true, false }) {
			for (const auto& [taste, list] : lists) {
				if (std::ranges::any_of(*list, [&](RE::TESForm* a_entry) { return Matches(a_entry, a_item, outright); })) {
					return taste;
				}
			}
		}
		const auto keywords = a_item->As<RE::BGSKeywordForm>();
		return keywords && keywords->HasKeyword(Forms::Get().giftInsulting) ? Taste::kInsulted : Taste::kNeutral;
	}

	void OnGiven(RE::Actor* a_actor, const Candidates::Candidate& a_candidate, RE::TESForm* a_item)
	{
		const auto& r = Rules::Get();
		const auto  taste = TasteOf(a_candidate, a_item);
		if (taste == Taste::kInsulted) {
			Romance::ModApproval(a_actor, -r.giftInsultCost, "an insulting gift");  // every one counts
		} else if (taste == Taste::kDisliked) {
			Romance::ModApproval(a_actor, -r.giftDislikedCost, "a gift they dislike");
		} else {
			auto& s = State::Of(a_actor->GetFormID());
			if (State::Today() - s.lastGiftDay >= 1.0f) {  // one gift a day counts, so a pile of gifts isn't a shortcut
				s.lastGiftDay = State::Today();
				const int gain = taste == Taste::kLoved ? r.giftLovedGain : taste == Taste::kLiked ? r.giftLikedGain : r.giftGain;
				Romance::ModApproval(a_actor, gain, taste == Taste::kLoved ? "a gift they love" : taste == Taste::kLiked ? "a gift they like" : "a gift");
			}
		}
		Romance::Contact(a_actor);
		if (taste != Taste::kInsulted && taste != Taste::kDisliked && Romance::GetStage(a_actor) >= Stage::kCourting) {
			Romance::Strayed(a_actor, "a gift for someone else");  // a gift to a friend is just a gift, an insult no courting
		}
		if (g_pending.get().get() != a_actor || Strength(taste) > Strength(g_pendingTaste)) {
			g_pending = a_actor->GetHandle();
			g_pendingTaste = taste;
		}
		const auto ui = RE::UI::GetSingleton();
		if (!ui->IsMenuOpen(RE::GiftMenu::MENU_NAME) && !ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) && !ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
			OnMenuClosed(""sv);  // given without a menu (a script): answer now
		}
	}

	void OnMenuClosed(std::string_view a_menu)
	{
		if (!g_pending) {
			return;
		}
		// a line said while a dialogue is open never plays: wait for it to close too
		if (RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME) && a_menu != RE::DialogueMenu::MENU_NAME) {
			return;
		}
		if (const auto a = g_pending.get()) {
			World::SayMoment(a.get(), static_cast<Moment>(Rank(Moment::kGiftInsulted) + Rank(g_pendingTaste)), 1.5f);
		}
		g_pending = {};
		g_pendingTaste = Taste::kNeutral;
	}

	void OnGameLoaded()
	{
		g_pending = {};
		g_pendingTaste = Taste::kNeutral;
	}
}
