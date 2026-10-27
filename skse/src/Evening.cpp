#include "Evening.h"

#include "Candidates.h"
#include "Rules.h"
#include "Scenes.h"
#include "Script.h"
#include "World.h"

namespace Evening
{
	using namespace IRF;

	namespace
	{
		constexpr auto kFolder = "Data/SKSE/Plugins/ImmersiveRomanceFramework/Evenings"sv;

		std::unordered_map<RE::FormID, RE::TESObjectREFR*> g_spots;  // a place (the inn's, its parent's, its town) -> the inn's exit door

		// game thread (SKSE tasks)
		RE::ActorHandle g_pending;  // her "date" line was said; the evening starts when the talk closes

		void Run(RE::Actor* a_her)
		{
			const auto door = Spot(a_her);
			const auto extra = door ? door->extraList.GetByType<RE::ExtraTeleport>() : nullptr;
			const auto data = extra ? extra->teleportData : nullptr;
			const auto outside = data ? data->linkedDoor.get() : RE::TESObjectREFRPtr{};
			if (!outside) {
				logger::warn("Evening: no way out for {} here (her door has no teleport)", a_her->GetDisplayFullName());
				return;
			}
			if (!Scenes::Begin(Scenes::Kind::kEvening, { a_her }, {})) {
				return;  // another scene holds the stage: the evening's approval stands, no fade
			}
			// where the exit door puts you, as an offset from the door outside (Papyrus MoveTo takes offsets), facing out
			const auto offset = data->position - outside->GetPosition();
			const auto facing = data->rotation.z * 180.0f / std::numbers::pi_v<float>;
			logger::info("Evening: {} and you, {} hours, out at {:08X}", a_her->GetDisplayFullName(), Rules::Get().eveningHours, door->GetFormID());
			Script::CallStatic("IRF_Internal"sv, "EveningTogether"sv, a_her, outside.get(), offset.x, offset.y, offset.z, facing,
				Rules::Get().eveningHours);
		}
	}

	void Load()
	{
		g_spots.clear();
		std::error_code ec;
		if (!std::filesystem::is_directory(kFolder, ec)) {
			return;
		}
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::directory_iterator(kFolder, ec)) {
			if (entry.path().extension() == ".json") {
				files.push_back(entry.path());
			}
		}
		std::ranges::sort(files);  // later files win a town
		for (const auto& path : files) {
			std::ifstream in(path);
			const auto    j = nlohmann::json::parse(in, nullptr, false, true);
			if (j.is_discarded()) {
				logger::error("Evening: {} is not valid JSON", path.filename().string());
				continue;
			}
			for (const auto& inn : j.value("inns", nlohmann::json::array())) {
				const auto place = Candidates::ParseForm(inn.value("inn", ""s));
				const auto exit = Candidates::ParseForm(inn.value("door", ""s));
				const auto loc = place ? place->As<RE::BGSLocation>() : nullptr;
				const auto door = exit ? exit->As<RE::TESObjectREFR>() : nullptr;
				if (!loc || !door) {
					logger::warn("Evening: {}: {} or {} isn't loaded", path.filename().string(), inn.value("inn", ""s), inn.value("door", ""s));
					continue;
				}
				for (const auto key : { loc, loc->parentLoc, Candidates::TownOf(loc) }) {
					if (key) {
						g_spots[key->GetFormID()] = door;
					}
				}
			}
		}
		logger::info("Evening: {} places with a spot to end an evening", g_spots.size());
	}

	RE::TESObjectREFR* Spot(RE::Actor* a_her)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto loc = player->GetCurrentLocation();
		const auto candidate = Candidates::Find(a_her);
		if (candidate && candidate->attributes.eveningHome && candidate->community && !a_her->IsPlayerTeammate() &&
			Candidates::TownOf(loc) == candidate->community) {
			return candidate->attributes.eveningHome;  // her own town: you see her home
		}
		for (auto place = loc; place; place = place->parentLoc) {
			if (const auto it = g_spots.find(place->GetFormID()); it != g_spots.end()) {
				return it->second;
			}
		}
		if (const auto town = Candidates::TownOf(loc)) {
			if (const auto it = g_spots.find(town->GetFormID()); it != g_spots.end()) {
				return it->second;
			}
		}
		return nullptr;
	}

	void Begin(RE::Actor* a_her)
	{
		if (RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
			g_pending = a_her->GetHandle();
		}
	}

	void OnDialogueClosed()
	{
		const auto her = g_pending.get();
		g_pending = {};
		if (her) {
			Run(her.get());
		}
	}

	void PassTime(float a_hours)
	{
		const auto calendar = RE::Calendar::GetSingleton();
		if (!calendar || !calendar->gameHour || !calendar->gameDaysPassed) {
			return;
		}
		const float hours = std::min(a_hours, 23.9f - calendar->gameHour->value);  // the evening ends before midnight
		if (hours > 0.0f) {
			calendar->gameHour->value += hours;
			calendar->gameDaysPassed->value += hours / 24.0f;
		}
	}

	void Done(RE::Actor* a_her)
	{
		World::SayMoment(a_her, Moment::kDateAfter, 0.5f);  // "I had a great time."
		Scenes::End(Scenes::Kind::kEvening);
		if (!a_her->IsPlayerTeammate()) {
			Script::Call(a_her, "Actor"sv, "EvaluatePackage"sv);  // back to her day: at this hour, home
		}
	}

	void OnGameLoaded()
	{
		g_pending = {};
	}
}
