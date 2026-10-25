#include "Gifts.h"

#include "Forms.h"
#include "Romance.h"
#include "Rules.h"
#include "State.h"

// The gift menu: IRF_GiveTopic's line is blank and the talk goes on; as it plays the gift menu opens over the
// conversation. Closed without a gift, the player is back in the conversation; once something is picked it's handed
// over, the gift menu and the conversation close, and her goodbye is the answer (closing the talk from the DLL alone
// makes an NPC say an ordinary goodbye, so IRF_GiftAnswerTopic sits in a quest above every quest's goodbyes). The gift
// menu's own transfer never runs: its ItemSelect callback (the game's, through the menu's fxDelegate) is wrapped.
//
// Gold: the game leaves gold out of the gift menu, so a row for it is added at the end of the list once the game has
// filled it. Only the on-screen list gets the row, never the game's own item data, so nothing the game frees on closing
// is ours. The callbacks that look up the highlighted row (ItemSelect, RequestItemCardInfo, UpdateItem3D) are wrapped:
// on the gold row they're answered here, with an entry of our own for the coins (the card and the 3D coin).

namespace Gifts
{
	using namespace IRF;

	namespace
	{
		constexpr auto       kFolder = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Gifts"sv;
		constexpr RE::FormID kGold = 0xF;          // Gold001
		constexpr auto       kGoldMark = "irfGold";  // on the gold row's entry
		constexpr int        kFillTries = 3;         // UI tasks to wait for the game to fill the list

		using Clock = std::chrono::steady_clock;
		constexpr auto kOnce = std::chrono::seconds(2);  // the line's begin and end events open one menu

		// game thread (events and tasks)
		Clock::time_point g_opened;
		RE::ActorHandle   g_answered;  // whose IRF_GiftFaction holds an answer, cleared when the next talk opens

		// UI thread
		RE::FxDelegateHandler::CallbackFn*      g_itemSelect = nullptr;
		RE::FxDelegateHandler::CallbackFn*      g_cardInfo = nullptr;
		RE::FxDelegateHandler::CallbackFn*      g_item3D = nullptr;
		std::unique_ptr<RE::InventoryEntryData> g_goldEntry;  // the gold row's coins, for the card and the 3D model

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

		void Refuse(const char* a_text)
		{
			RE::PlaySound("MAGFailShoutSD");
			RE::SendHUDMessage::ShowHUDMessage(a_text);
		}

		bool IsAsleep(RE::Actor* a_actor)
		{
			const auto state = a_actor->AsActorState()->GetSitSleepState();
			return state >= RE::SIT_SLEEP_STATE::kWantToSleep && state <= RE::SIT_SLEEP_STATE::kWantToWake;
		}

		std::int32_t CountOf(RE::Actor* a_actor, RE::TESBoundObject* a_object)
		{
			const auto counts = a_actor->GetInventoryCounts([a_object](RE::TESBoundObject& a_item) { return &a_item == a_object; });
			const auto it = counts.find(a_object);
			return it != counts.end() ? it->second : 0;
		}

		// The extra list, while the player still holds it under this item.
		RE::ExtraDataList* StillHeld(RE::Actor* a_player, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra)
		{
			const auto changes = a_extra ? a_player->GetInventoryChanges() : nullptr;
			if (!changes || !changes->entryList) {
				return nullptr;
			}
			for (const auto entry : *changes->entryList) {
				if (entry && entry->object == a_object && entry->extraLists) {
					for (const auto extra : *entry->extraLists) {
						if (extra == a_extra) {
							return a_extra;
						}
					}
				}
			}
			return nullptr;
		}

		// Game thread (an SKSE task): a_count of a_object from the player to a candidate. Her answer becomes her goodbye.
		void Give(RE::Actor* a_actor, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra, std::int32_t a_count)
		{
			const auto candidate = Candidates::Find(a_actor);
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto held = a_object ? CountOf(player, a_object) : 0;
			const auto count = std::min(a_count, held);
			if (!candidate || count <= 0) {
				return;
			}
			if (IsAsleep(a_actor)) {
				RE::SendHUDMessage::ShowHUDMessage(std::format("{} is asleep.", a_actor->GetDisplayFullName()).c_str());
				return;
			}
			const auto& r = Rules::Get();
			const auto  taste = TasteOf(*candidate, a_object, count);
			logger::info("Gifts: {} x{} to {}: taste {}", a_object->GetName(), count, a_actor->GetDisplayFullName(), Rank(taste));
			if (taste != Taste::kInsulted) {  // an insult she won't take
				player->RemoveItem(a_object, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, StillHeld(player, a_object, a_extra), a_actor);
				if (const auto moved = held - CountOf(player, a_object); moved < count) {  // the extra list held fewer
					player->RemoveItem(a_object, count - moved, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, a_actor);
				}
			}
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
				if (Romance::GetStage(a_actor) >= Stage::kCourting) {
					Romance::Strayed(a_actor, "a gift for someone else");  // a gift to a friend is just a gift
				}
			}
			Romance::Contact(a_actor);
			a_actor->AddToFaction(Forms::Get().gift, static_cast<std::int8_t>(Rank(taste) + 1));
			g_answered = a_actor->GetHandle();
		}

		// A gift picked: handed over now, then the gift menu and the conversation close and she says her goodbye.
		void Pick(RE::RefHandle a_receiver, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra, std::int32_t a_count)
		{
			SKSE::GetTaskInterface()->AddTask([=] {
				const auto ref = RE::TESObjectREFR::LookupByHandle(a_receiver);
				if (const auto actor = ref ? ref->As<RE::Actor>() : nullptr) {
					Give(actor, a_object, a_extra, a_count);
				}
				const auto queue = RE::UIMessageQueue::GetSingleton();
				queue->AddMessage(RE::GiftMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
				queue->AddMessage(RE::DialogueMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			});
		}

		RE::GiftMenu* Menu()
		{
			const auto menu = RE::UI::GetSingleton()->GetMenu<RE::GiftMenu>();
			return menu && menu->IsPlayerGifting() ? menu.get() : nullptr;
		}

		bool Member(const RE::GFxValue& a_object, const char* a_name, RE::GFxValue* a_value)
		{
			return a_object.IsObject() && a_object.GetMember(a_name, a_value);
		}

		RE::ItemList* List(RE::GiftMenu* a_menu)
		{
			return a_menu ? a_menu->GetRuntimeData().itemList : nullptr;
		}

		// The highlighted row is the gold row
		bool GoldSelected(RE::ItemList* a_list)
		{
			RE::GFxValue entry, mark;
			return a_list && g_goldEntry && a_list->root.GetMember("selectedEntry", &entry) && Member(entry, kGoldMark, &mark) && mark.IsBool() &&
			       mark.GetBool();
		}

		// UI thread: the item picked goes to the receiver (an SKSE task: it moves items and changes factions)
		void OnItemSelect(const RE::FxDelegateArgs& a_args)
		{
			const auto   list = List(Menu());
			std::int32_t amount = a_args.GetArgCount() > 0 && a_args[0].IsNumber() ? static_cast<std::int32_t>(a_args[0].GetNumber()) : 1;
			if (GoldSelected(list)) {
				if (const auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold); gold && amount > 0) {
					logger::info("Gifts: gold x{} picked", amount);
					Pick(RE::GiftMenu::GetReceiverRefHandle(), gold, nullptr, amount);
				}
				return;
			}
			const auto item = list ? list->GetSelectedItem() : nullptr;
			const auto entry = item ? item->data.objDesc : nullptr;
			if (!entry || !entry->object) {
				if (g_itemSelect) {
					g_itemSelect(a_args);
				}
				return;
			}
			if (entry->IsQuestObject()) {
				SKSE::GetTaskInterface()->AddTask([] { Refuse("You can't give that away."); });
				return;
			}
			if (!entry->IsOwnedBy(RE::PlayerCharacter::GetSingleton(), true)) {
				SKSE::GetTaskInterface()->AddTask([] { Refuse("Stolen goods can't be given away."); });
				return;
			}
			if (amount <= 0) {
				return;
			}
			const auto extra = entry->extraLists && !entry->extraLists->empty() ? entry->extraLists->front() : nullptr;
			Pick(RE::GiftMenu::GetReceiverRefHandle(), entry->object, extra, amount);
		}

		// The card for the highlighted row: the coins' own on the gold row
		void OnCardInfo(const RE::FxDelegateArgs& a_args)
		{
			const auto menu = Menu();
			const auto card = menu ? menu->GetRuntimeData().itemCard : nullptr;
			if (card && GoldSelected(List(menu))) {
				card->SetItem(g_goldEntry.get(), false);
				menu->GetRuntimeData().root.Invoke("UpdateItemCardInfo", nullptr, &card->obj, 1);
				return;
			}
			if (g_cardInfo) {
				g_cardInfo(a_args);
			}
		}

		// The 3D model of the highlighted row: a coin on the gold row
		void OnItem3D(const RE::FxDelegateArgs& a_args)
		{
			if (GoldSelected(List(Menu()))) {
				const auto manager = RE::Inventory3DManager::GetSingleton();
				manager->UnloadInventoryItem();
				manager->LoadInventoryItem(g_goldEntry.get());
				return;
			}
			if (g_item3D) {
				g_item3D(a_args);
			}
		}

		// The filter flag for gold's row: a misc item's, or the MISC category's
		std::optional<double> MiscFlag(RE::GiftMenu* a_menu, RE::ItemList* a_list)
		{
			RE::GFxValue entry, flag;
			for (std::uint32_t i = 0; i < a_list->items.size(); ++i) {
				const auto item = a_list->items[i];
				const auto object = item && item->data.objDesc ? item->data.objDesc->object : nullptr;
				if (object && object->Is(RE::FormType::Misc) && a_list->entryList.GetElement(i, &entry) && Member(entry, "filterFlag", &flag) &&
					flag.IsNumber()) {
					return flag.GetNumber();
				}
			}
			RE::GFxValue lists, categories, entries, text;
			auto&        root = a_menu->GetRuntimeData().root;
			if (Member(root, "InventoryLists_mc", &lists) && (Member(lists, "CategoriesList", &categories) || Member(lists, "_CategoriesList", &categories)) &&
				(Member(categories, "entryList", &entries) || Member(categories, "EntriesA", &entries)) && entries.IsArray()) {
				for (std::uint32_t i = 0; i < entries.GetArraySize(); ++i) {
					if (entries.GetElement(i, &entry) && Member(entry, "text", &text) && text.IsString() &&
						std::string_view{ text.GetString() }.find("MISC") != std::string_view::npos && Member(entry, "flag", &flag) && flag.IsNumber()) {
						return flag.GetNumber();
					}
				}
			}
			return std::nullopt;
		}

		// Gold's row at the end of the list: a copy of the game's first row with gold's name, count and category
		void AddGold(int a_tries)
		{
			const auto menu = Menu();
			const auto list = List(menu);
			const auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold);
			if (!list || !gold || !list->view) {
				return;
			}
			const auto count = RE::PlayerCharacter::GetSingleton()->GetItemCount(gold);
			if (count <= 0) {
				return;
			}
			RE::GFxValue first;
			if (list->items.empty() || !list->entryList.IsArray() || !list->entryList.GetElement(0, &first)) {
				if (a_tries > 1) {  // not filled yet
					SKSE::GetTaskInterface()->AddUITask([a_tries] { AddGold(a_tries - 1); });
				} else {
					logger::info("Gifts: the gift menu's list is empty, no gold row");
				}
				return;
			}
			const auto flag = MiscFlag(menu, list);
			if (!flag) {
				logger::warn("Gifts: no MISC category found, no gold row");
				return;
			}
			RE::GFxValue row;
			list->view->CreateObject(&row);
			first.VisitMembers([&](const char* a_name, const RE::GFxValue& a_value) { row.SetMember(a_name, a_value); });
			g_goldEntry = std::make_unique<RE::InventoryEntryData>(gold, count);
			const std::string text = gold->GetName();
			row.SetMember("text", RE::GFxValue(text.c_str()));
			row.SetMember("count", RE::GFxValue(static_cast<double>(count)));
			row.SetMember("filterFlag", RE::GFxValue(*flag));
			row.SetMember("equipState", RE::GFxValue(0.0));
			row.SetMember("favorite", RE::GFxValue(false));
			row.SetMember("enabled", RE::GFxValue(true));
			row.SetMember(kGoldMark, RE::GFxValue(true));
			list->entryList.PushBack(row);
			RE::GFxValue lists;
			if (Member(menu->GetRuntimeData().root, "InventoryLists_mc", &lists)) {
				lists.Invoke("InvalidateListData");
			}
			logger::info("Gifts: gold row added ({} coins)", count);
		}

		void WrapCallback(RE::GiftMenu* a_menu, const char* a_name, RE::FxDelegateHandler::CallbackFn* a_ours, RE::FxDelegateHandler::CallbackFn*& a_original)
		{
			const auto defn = a_menu->fxDelegate->callbacks.Get(RE::GString(a_name));
			if (!defn) {
				logger::warn("Gifts: the gift menu has no {}", a_name);
				return;
			}
			if (defn->callback != a_ours) {
				a_original = defn->callback;
				defn->callback = a_ours;
			}
		}

		// UI thread: the gift menu opened for the player
		void Wrap()
		{
			const auto menu = Menu();
			if (!menu || !menu->fxDelegate || !menu->fxDelegate->callbacks.Get(RE::GString("ItemSelect"))) {
				return;
			}
			WrapCallback(menu, "ItemSelect", OnItemSelect, g_itemSelect);
			WrapCallback(menu, "RequestItemCardInfo", OnCardInfo, g_cardInfo);
			WrapCallback(menu, "UpdateItem3D", OnItem3D, g_item3D);
			g_goldEntry.reset();  // the last menu's coins (closed, its 3D unloaded)
			AddGold(kFillTries);
		}

		// The give line, as it plays (one compare for every other line anyone says)
		struct TopicSink : RE::BSTEventSink<RE::TESTopicInfoEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* a_event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
			{
				if (!a_event || !a_event->speakerRef) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto info = RE::TESForm::LookupByID<RE::TESTopicInfo>(a_event->topicInfoFormID);
				const auto actor = info && info->parentTopic == Forms::Get().giveTopic ? a_event->speakerRef->As<RE::Actor>() : nullptr;
				if (!actor) {
					return RE::BSEventNotifyControl::kContinue;
				}
				SKSE::GetTaskInterface()->AddTask([handle = actor->GetHandle()] {
					const auto now = Clock::now();
					if (now - g_opened < kOnce) {
						return;  // the line's other event
					}
					if (const auto a = handle.get(); a && !a->IsDead() && !a->IsInCombat()) {
						g_opened = now;
						logger::info("Gifts: the gift menu on {}", a->GetDisplayFullName());
						RE::GiftMenu::OpenMenuAsGifter(a.get(), [](std::int32_t) {}, nullptr, false);
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// The gift menu opening: wrapped. A conversation opening: the last gift's answer is spent.
		struct MenuSink : RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event || !a_event->opening) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->menuName == RE::GiftMenu::MENU_NAME) {
					SKSE::GetTaskInterface()->AddUITask(Wrap);
				} else if (a_event->menuName == RE::DialogueMenu::MENU_NAME && g_answered) {
					SKSE::GetTaskInterface()->AddTask([] {
						if (const auto a = g_answered.get()) {
							a->AddToFaction(Forms::Get().gift, 0);
						}
						g_answered = {};
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void TagInsulting()
		{
			const auto      keyword = Forms::Get().giftInsulting;
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
	}

	void Load()
	{
		TagInsulting();
		static TopicSink topicSink;
		static MenuSink  menuSink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESTopicInfoEvent>(&topicSink);
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
	}

	Taste TasteOf(const Candidates::Candidate& a_candidate, RE::TESForm* a_item, std::int32_t a_count)
	{
		const auto& r = Rules::Get();
		if (a_item->GetFormID() == kGold) {  // gold goes by the amount
			return a_count >= r.giftGoldLoved ? Taste::kLoved : a_count >= r.giftGoldLiked ? Taste::kLiked : Taste::kDisliked;
		}
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

	void OnGameLoaded()
	{
		g_opened = {};
		g_answered = {};
	}
}
