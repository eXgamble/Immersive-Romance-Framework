#include "World.h"

#include "Candidates.h"
#include "Forms.h"
#include "Rules.h"
#include "Scenes.h"
#include "Script.h"

namespace World
{
	using namespace IRF;

	bool Sees(RE::Actor* a_watcher, RE::Actor* a_target, float a_distance)
	{
		if (!a_watcher || !a_target || a_watcher == a_target || a_watcher->IsDead() || !a_watcher->Is3DLoaded()) {
			return false;
		}
		if (a_watcher->GetPosition().GetDistance(a_target->GetPosition()) > a_distance) {
			return false;
		}
		return a_watcher->RequestDetectionLevel(a_target) > 0;
	}

	bool InSight(RE::Actor* a_watcher, RE::Actor* a_target, float a_distance)
	{
		if (!a_watcher || !a_target || a_watcher == a_target || a_watcher->IsDead() || !a_watcher->Is3DLoaded() || !a_target->Is3DLoaded()) {
			return false;
		}
		const auto eye = a_watcher->GetLookingAtLocation();
		const auto head = a_target->GetLookingAtLocation();
		auto       to = head - eye;
		const float distance = to.Length();
		if (distance > a_distance || distance < 1.0f) {
			return false;
		}
		const float heading = a_watcher->GetAngleZ();
		to /= distance;
		if (to.x * std::sin(heading) + to.y * std::cos(heading) <= 0.0f) {
			return false;  // behind her
		}
		const auto cell = a_watcher->GetParentCell();
		const auto world = cell ? cell->GetbhkWorld() : nullptr;
		if (!world) {
			return false;
		}
		const float     scale = RE::bhkWorld::GetWorldScale();
		RE::bhkPickData pick;
		pick.rayInput.from = RE::hkVector4(eye.x * scale, eye.y * scale, eye.z * scale, 0.0f);
		pick.rayInput.to = RE::hkVector4(head.x * scale, head.y * scale, head.z * scale, 0.0f);
		RE::CFilter filter;
		a_watcher->GetCollisionFilterInfo(filter);
		pick.rayInput.filterInfo.filter = (filter.filter & 0xFFFF0000) | std::to_underlying(RE::COL_LAYER::kLOS);
		world->PickObject(pick);
		// clear, or the ray stopped on their own body
		return !pick.rayOutput.HasHit() || pick.rayOutput.hitFraction * distance >= distance - 150.0f;
	}

	std::vector<RE::Actor*> Witnesses(RE::Actor* a_target, float a_distance)
	{
		std::vector<RE::Actor*> out;
		const auto              lists = RE::ProcessLists::GetSingleton();
		if (!lists) {
			return out;
		}
		for (const auto& handle : lists->highActorHandles) {  // skse-lint: ok once per moment (kiss, crime, gift, talk), never per frame
			const auto actor = handle.get();
			if (!actor || actor.get() == a_target || actor->IsPlayerRef() || actor->IsPlayerTeammate()) {
				continue;
			}
			if (Sees(actor.get(), a_target, a_distance)) {
				out.push_back(actor.get());
			}
		}
		return out;
	}

	bool PrivateEnough(RE::Actor* a_partner, int a_modesty)
	{
		if (a_modesty <= 0) {
			return true;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		for (const auto witness : Witnesses(player, 2048.0f)) {
			if (witness != a_partner) {
				return false;
			}
		}
		if (a_modesty == 1) {
			return true;
		}
		const auto cell = player->GetParentCell();
		if (!cell || !cell->IsInteriorCell()) {
			return false;
		}
		if (a_modesty == 2) {
			return true;
		}
		const auto& f = Forms::Get();
		const auto  loc = player->GetCurrentLocation();
		return loc && (loc->HasKeyword(f.locPlayerHouse) || loc->HasKeyword(f.locHouse) || loc->HasKeyword(f.locDwelling));
	}

	bool Roll(int a_percent)
	{
		static std::mt19937 rng{ std::random_device{}() };
		return std::uniform_int_distribution<int>(0, 99)(rng) < a_percent;
	}

	void SayMoment(RE::Actor* a_speaker, Moment a_moment, float a_delay)
	{
		const auto& f = Forms::Get();
		// a delayed line may wait for its speaker to arrive (a follower comes through a load door a moment after you)
		if (!a_speaker || a_speaker->IsDead() || (a_delay <= 0.0f && !a_speaker->Is3DLoaded())) {
			return;
		}
		if (!Scenes::MaySay(a_speaker, a_moment)) {
			return;  // she's in a scene: a line from outside it would take her moment
		}
		a_speaker->AddToFaction(f.moment, static_cast<std::int8_t>(a_moment));
		if (a_delay > 0.0f) {
			Script::CallStatic("IRF_Internal"sv, "SayLater"sv, static_cast<RE::TESObjectREFR*>(a_speaker), f.say, a_delay);
		} else {
			Script::Say(a_speaker, f.say);
		}
		logger::info("World: {} says their line for moment {}", a_speaker->GetDisplayFullName(), Rank(a_moment));
	}

	Moment MomentForPlace(RE::BGSLocation* a_location, RE::BGSLocation* a_home)
	{
		if (!a_location) {
			return Moment::kNone;
		}
		const auto& f = Forms::Get();
		if (a_home && Candidates::TownOf(a_location) == a_home) {
			return Moment::kHomeTown;
		}
		if (a_location->HasKeyword(f.locNordicRuin)) {
			return Moment::kNordicRuin;
		}
		if (a_location->HasKeyword(f.locDwarvenRuin)) {
			return Moment::kDwarvenRuin;
		}
		if (a_location->HasKeyword(f.locCave)) {
			return Moment::kCave;
		}
		if (a_location->HasKeyword(f.locDungeon)) {
			return Moment::kDungeon;
		}
		if (a_location->HasKeyword(f.locInn)) {
			return Moment::kInn;
		}
		if (a_location->HasKeyword(f.locCity) || a_location->HasKeyword(f.locTown)) {
			return Moment::kTown;
		}
		return Moment::kNone;
	}

	bool Evening()
	{
		const auto calendar = RE::Calendar::GetSingleton();
		const auto hour = calendar ? calendar->GetHour() : 12.0f;
		return hour >= 18.0f && hour <= 23.5f;
	}

	bool DateTime()
	{
		const auto  calendar = RE::Calendar::GetSingleton();
		const auto  hour = calendar ? calendar->GetHour() : 12.0f;
		const auto& r = Rules::Get();
		const auto  player = RE::PlayerCharacter::GetSingleton();
		if (hour < r.dateFromHour || hour > r.dateToHour || player->IsInCombat()) {
			return false;
		}
		const auto loc = player->GetCurrentLocation();
		if (!loc) {
			return false;  // the wilds
		}
		if (Candidates::TownOf(loc)) {
			return true;
		}
		const auto& f = Forms::Get();
		for (const auto keyword : { f.locCity, f.locTown, f.locSettlement, f.locInn, f.locPlayerHouse, f.locHouse, f.locDwelling }) {
			if (keyword && loc->HasKeyword(keyword)) {
				return true;
			}
		}
		return false;
	}

	bool SamePlace(RE::Actor* a_a, RE::Actor* a_b)
	{
		const auto ca = a_a->GetParentCell();
		const auto cb = a_b->GetParentCell();
		if (!ca || !cb) {
			return false;
		}
		if (ca->IsInteriorCell() || cb->IsInteriorCell()) {
			return ca == cb;
		}
		return a_a->GetWorldspace() == a_b->GetWorldspace();
	}

	void Kiss(RE::Actor* a_partner)
	{
		// Game.FadeOutGame(abFadingOut, abBlackFade, afSecsBeforeFade, afFadeDuration): a short fade in
		Script::CallStatic("Game"sv, "FadeOutGame"sv, false, true, 0.0f, 1.5f);
		logger::info("World: a kiss with {}", a_partner->GetDisplayFullName());
	}
}
