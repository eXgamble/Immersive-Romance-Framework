#include "Crimes.h"

#include "Candidates.h"
#include "Romance.h"
#include "Rules.h"
#include "World.h"

namespace Crimes
{
	using namespace IRF;

	namespace
	{
		std::set<const RE::Crime*>              g_seen;  // crimes already judged (each crime calls both hooks)
		std::atomic_bool                        g_queued{ false };
		std::unordered_map<RE::FormID, int>     g_stolen;  // stolen units of an item in your pack, as last counted

		int Severity(std::uint32_t a_type)
		{
			switch (a_type) {
			case RE::PackageNS::CRIME_TYPES::kSteal:
			case RE::PackageNS::CRIME_TYPES::kTrespass:
				return 1;
			case RE::PackageNS::CRIME_TYPES::kPickpocket:
				return 2;
			case RE::PackageNS::CRIME_TYPES::kAttack:
				return 4;
			case RE::PackageNS::CRIME_TYPES::kMurder:
				return 10;
			default:
				return 0;
			}
		}

		// RE::Crime +0x08 holds two 32-bit handles: low = the victim (or the stolen item's reference), high = the
		// criminal (measured 2026-10-24 on 1.6.1170)
		RE::RefHandle Criminal(const RE::Crime* a_crime)
		{
			return static_cast<RE::RefHandle>(a_crime->unk08 >> 32);
		}

		RE::RefHandle Victim(const RE::Crime* a_crime)
		{
			return static_cast<RE::RefHandle>(a_crime->unk08 & 0xFFFFFFFF);
		}

		void Judge(RE::Actor* a_witness, std::uint32_t a_type)
		{
			const auto candidate = Candidates::Find(a_witness);
			const auto stage = Romance::GetStage(a_witness);
			if (!candidate || stage < Stage::kFriend || stage > Stage::kMarried) {
				return;
			}
			const int alignment = candidate->attributes.alignment;
			const int amount = std::abs(alignment) * Severity(a_type) * Rules::Get().crimeScale;
			if (alignment == 0 || amount == 0) {
				return;
			}
			logger::info("Crimes: {} saw a crime (type {}), alignment {}", a_witness->GetDisplayFullName(), a_type, alignment);
			if (alignment > 0) {
				World::SayMoment(a_witness, Moment::kCrimeDisapprove);
				Romance::Hurt(a_witness, { amount, false }, Why::kCrime, "saw you break the law");
			} else {
				World::SayMoment(a_witness, Moment::kCrimeApprove);
				Romance::ModApproval(a_witness, amount, "liked your crime");
			}
		}

		void Process()
		{
			g_queued = false;
			const auto lists = RE::ProcessLists::GetSingleton();
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!lists || !player) {
				return;
			}
			std::set<const RE::Crime*> live;
			for (std::uint32_t type = 0; type < RE::PackageNS::CRIME_TYPES::kTotal; ++type) {
				const auto list = lists->globalCrimes[type];
				if (!list) {
					continue;
				}
				// thefts and pickpocketing are judged from the item (OnItemTaken), which also sees the cheap ones
				if (type == RE::PackageNS::CRIME_TYPES::kSteal || type == RE::PackageNS::CRIME_TYPES::kPickpocket) {
					continue;
				}
				for (const auto crime : *list) {
					if (!crime) {
						continue;
					}
					live.insert(crime);
					if (g_seen.contains(crime)) {
						continue;
					}
					g_seen.insert(crime);
					RE::NiPointer<RE::Actor> criminal;
					if (!RE::LookupReferenceByHandle(Criminal(crime), criminal) || criminal.get() != player) {
						continue;
					}
					std::set<RE::Actor*> witnesses;
					crime->lock.LockForRead();
					for (const auto& handle : crime->actorsKnowOfCrime) {
						if (const auto w = handle.get()) {
							witnesses.insert(w.get());
						}
					}
					crime->lock.UnlockForRead();
					for (const auto partner : Romance::Partners()) {
						if (partner->IsPlayerTeammate()) {
							witnesses.insert(partner);  // followers are never on the game's list
						}
					}
					// the victim of an attack judges it as an attack on them (Events), not as a crime they saw
					RE::NiPointer<RE::TESObjectREFR> victim;
					RE::LookupReferenceByHandle(Victim(crime), victim);
					witnesses.erase(victim ? victim->As<RE::Actor>() : nullptr);
					std::erase_if(witnesses, [&](RE::Actor* a_witness) { return crime->owner && (crime->owner == a_witness->GetActorBase() || crime->owner == a_witness); });
					logger::info("Crimes: type {} victim {} owner {:08X}, {} witnesses", type, victim ? victim->GetDisplayFullName() : "none",
						crime->owner ? crime->owner->GetFormID() : 0, witnesses.size());
					const auto distance = Rules::Get().witnessDistance;
					for (const auto witness : witnesses) {
						// the game's list holds everyone alerted (even a cow 50 m away): they must really see it
						if (Candidates::Find(witness) && World::Sees(witness, player, distance)) {
							Judge(witness, type);
						}
					}
				}
			}
			std::erase_if(g_seen, [&](const RE::Crime* a_crime) { return !live.contains(a_crime); });
		}

		// Units of a_item in the player's pack that still belong to someone else
		int StolenUnits(RE::PlayerCharacter* a_player, RE::FormID a_item)
		{
			const auto changes = a_player->GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return 0;
			}
			int units = 0;
			for (const auto entry : *changes->entryList) {
				if (!entry || !entry->object || entry->object->GetFormID() != a_item || !entry->extraLists) {
					continue;
				}
				for (const auto xList : *entry->extraLists) {
					const auto owner = xList ? xList->GetOwner() : nullptr;
					if (!owner || owner == a_player || owner == a_player->GetActorBase()) {
						continue;
					}
					if (const auto faction = owner->As<RE::TESFaction>(); faction && a_player->IsInFaction(faction)) {
						continue;
					}
					units += xList->GetCount();
				}
			}
			return units;
		}

		void Queue()
		{
			if (!g_queued.exchange(true)) {
				SKSE::GetTaskInterface()->AddTask(Process);
			}
		}

		struct SetCrimeGold
		{
			static void Thunk(RE::PlayerCharacter* a_this, RE::TESFaction* a_faction, bool a_violent, std::uint32_t a_amount)
			{
				func(a_this, a_faction, a_violent, a_amount);
				Queue();
			}
			static inline REL::Relocation<decltype(Thunk)> func;
		};

		struct ModCrimeGold
		{
			static void Thunk(RE::PlayerCharacter* a_this, RE::TESFaction* a_faction, bool a_violent, std::int32_t a_amount)
			{
				func(a_this, a_faction, a_violent, a_amount);
				Queue();
			}
			static inline REL::Relocation<decltype(Thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		SetCrimeGold::func = vtbl.write_vfunc(0xB5, SetCrimeGold::Thunk);
		ModCrimeGold::func = vtbl.write_vfunc(0xB6, ModCrimeGold::Thunk);
		logger::info("Crimes: hooked the player's crime-gold functions");
	}

	void OnGameLoaded()
	{
		g_seen.clear();
		g_stolen.clear();
	}

	void OnItemTaken(RE::FormID a_from, RE::FormID a_item, std::int32_t a_count)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const int  now = StolenUnits(player, a_item);
		const auto it = g_stolen.find(a_item);
		// not counted since the load: assume the stolen units in the pack came with this pickup
		const int before = it != g_stolen.end() ? it->second : std::max(0, now - a_count);
		g_stolen[a_item] = now;
		if (now <= before) {
			return;
		}
		const auto from = a_from ? RE::TESForm::LookupByID<RE::Actor>(a_from) : nullptr;
		const auto type = from && !from->IsDead() ? RE::PackageNS::CRIME_TYPES::kPickpocket : RE::PackageNS::CRIME_TYPES::kSteal;
		const auto distance = Rules::Get().witnessDistance;
		for (const auto& candidate : Candidates::All()) {
			const auto actor = Candidates::ActorOf(candidate);
			if (actor && actor != from && actor->Is3DLoaded() && !actor->IsDead() && World::Sees(actor, player, distance)) {
				Judge(actor, type);
			}
		}
	}

	void OnItemGiven(RE::FormID a_item)
	{
		g_stolen.erase(a_item);  // recounted at the next pickup
	}
}
