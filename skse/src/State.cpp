#include "State.h"

namespace State
{
	namespace
	{
		std::unordered_map<RE::FormID, Candidate> g_candidates;
		std::unordered_map<RE::FormID, Reaction>  g_reactions;
		std::unordered_map<RE::FormID, Refusal>   g_refusals;
		std::string                               g_interest;
		std::unordered_map<RE::FormID, Child>     g_children;

		constexpr std::uint32_t kId = 'IRF0';
		constexpr std::uint32_t kRecord = 'IRFS';
		constexpr std::uint32_t kVersion = 6;

		template <class T>
		void Write(SKSE::SerializationInterface* a_intfc, const T& a_value)
		{
			a_intfc->WriteRecordData(&a_value, sizeof(T));
		}

		template <class T>
		bool Read(SKSE::SerializationInterface* a_intfc, T& a_value)
		{
			return a_intfc->ReadRecordData(&a_value, sizeof(T)) == sizeof(T);
		}

		void WriteSet(SKSE::SerializationInterface* a_intfc, const std::set<RE::FormID>& a_set)
		{
			Write(a_intfc, static_cast<std::uint32_t>(a_set.size()));
			for (const auto id : a_set) {
				Write(a_intfc, id);
			}
		}

		bool ReadSet(SKSE::SerializationInterface* a_intfc, std::set<RE::FormID>& a_set)
		{
			std::uint32_t count = 0;
			if (!Read(a_intfc, count)) {
				return false;
			}
			for (std::uint32_t i = 0; i < count; ++i) {
				RE::FormID id = 0, resolved = 0;
				if (!Read(a_intfc, id)) {
					return false;
				}
				if (a_intfc->ResolveFormID(id, resolved)) {
					a_set.insert(resolved);
				}
			}
			return true;
		}

		void OnSave(SKSE::SerializationInterface* a_intfc)
		{
			if (!a_intfc->OpenRecord(kRecord, kVersion)) {
				logger::error("State: could not open the co-save record");
				return;
			}
			Write(a_intfc, static_cast<std::uint32_t>(g_candidates.size()));
			for (const auto& [id, c] : g_candidates) {
				Write(a_intfc, id);
				Write(a_intfc, c.lastContactDay);
				Write(a_intfc, c.silentUntilDay);
				Write(a_intfc, c.hurt);
				Write(a_intfc, c.why);
				Write(a_intfc, c.who);
				Write(a_intfc, c.breakupDay);
				Write(a_intfc, c.unforgivable);
				Write(a_intfc, c.secret);
				Write(a_intfc, c.lockUntilDay);
				Write(a_intfc, c.lastTalkDay);
				Write(a_intfc, c.lastDateDay);
				Write(a_intfc, c.lastKissDay);
				Write(a_intfc, c.lastGiftDay);
				Write(a_intfc, c.killDay);
				Write(a_intfc, c.killsToday);
				Write(a_intfc, c.recent);
				Write(a_intfc, c.recentDay);
				Write(a_intfc, c.partnerXp);
				Write(a_intfc, c.appliedLevels);
				Write(a_intfc, c.profileApplied);
				Write(a_intfc, c.savedConfidence);
				Write(a_intfc, c.savedAggression);
				Write(a_intfc, c.pendingBreakup);
				Write(a_intfc, c.confrontPending);
				Write(a_intfc, c.confrontWorks);
				Write(a_intfc, c.waryUntilDay);
				Write(a_intfc, c.jealousUntilDay);
				Write(a_intfc, c.pleadsLeft);
				Write(a_intfc, c.nextPleadDay);
				WriteSet(a_intfc, c.knowSeeing);
				WriteSet(a_intfc, c.sawStraying);
			}
			Write(a_intfc, static_cast<std::uint32_t>(g_reactions.size()));
			for (const auto& [id, r] : g_reactions) {
				Write(a_intfc, id);
				Write(a_intfc, r.event);
				Write(a_intfc, r.untilDay);
			}
			Write(a_intfc, static_cast<std::uint32_t>(g_refusals.size()));
			for (const auto& [id, r] : g_refusals) {
				Write(a_intfc, id);
				Write(a_intfc, r.because);
				Write(a_intfc, r.untilDay);
			}
			Write(a_intfc, static_cast<std::uint32_t>(g_interest.size()));
			a_intfc->WriteRecordData(g_interest.data(), static_cast<std::uint32_t>(g_interest.size()));
			Write(a_intfc, static_cast<std::uint32_t>(g_children.size()));
			for (const auto& [id, c] : g_children) {
				Write(a_intfc, id);
				Write(a_intfc, c.lastSeenDay);
				Write(a_intfc, c.lastMoodDay);
			}
		}

		bool ReadCandidate(SKSE::SerializationInterface* a_intfc, Candidate& c)
		{
			return Read(a_intfc, c.lastContactDay) && Read(a_intfc, c.silentUntilDay) && Read(a_intfc, c.hurt) && Read(a_intfc, c.why) &&
			       Read(a_intfc, c.who) && Read(a_intfc, c.breakupDay) && Read(a_intfc, c.unforgivable) && Read(a_intfc, c.secret) &&
			       Read(a_intfc, c.lockUntilDay) && Read(a_intfc, c.lastTalkDay) && Read(a_intfc, c.lastDateDay) && Read(a_intfc, c.lastKissDay) && Read(a_intfc, c.lastGiftDay) &&
			       Read(a_intfc, c.killDay) && Read(a_intfc, c.killsToday) && Read(a_intfc, c.recent) && Read(a_intfc, c.recentDay) &&
			       Read(a_intfc, c.partnerXp) && Read(a_intfc, c.appliedLevels) && Read(a_intfc, c.profileApplied) &&
			       Read(a_intfc, c.savedConfidence) && Read(a_intfc, c.savedAggression) && Read(a_intfc, c.pendingBreakup) &&
			       Read(a_intfc, c.confrontPending) && Read(a_intfc, c.confrontWorks) && Read(a_intfc, c.waryUntilDay) &&
			       Read(a_intfc, c.jealousUntilDay) && Read(a_intfc, c.pleadsLeft) && Read(a_intfc, c.nextPleadDay) && ReadSet(a_intfc, c.knowSeeing) &&
			       ReadSet(a_intfc, c.sawStraying);
		}

		void OnLoad(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t type = 0, version = 0, length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (type != kRecord || version != kVersion) {
					continue;
				}
				std::uint32_t count = 0;
				Read(a_intfc, count);
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::FormID id = 0, resolved = 0;
					Candidate  c;
					if (!Read(a_intfc, id) || !ReadCandidate(a_intfc, c)) {
						logger::error("State: co-save record cut short");
						return;
					}
					if (a_intfc->ResolveFormID(id, resolved)) {
						g_candidates[resolved] = std::move(c);
					}
				}
				Read(a_intfc, count);
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::FormID id = 0, resolved = 0;
					Reaction   r;
					if (!Read(a_intfc, id) || !Read(a_intfc, r.event) || !Read(a_intfc, r.untilDay)) {
						return;
					}
					if (a_intfc->ResolveFormID(id, resolved)) {
						g_reactions[resolved] = r;
					}
				}
				Read(a_intfc, count);
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::FormID id = 0, resolved = 0, because = 0;
					Refusal    r;
					if (!Read(a_intfc, id) || !Read(a_intfc, because) || !Read(a_intfc, r.untilDay)) {
						return;
					}
					if (a_intfc->ResolveFormID(id, resolved) && a_intfc->ResolveFormID(because, r.because)) {
						g_refusals[resolved] = r;
					}
				}
				std::uint32_t size = 0;
				if (Read(a_intfc, size) && size < 64) {
					g_interest.resize(size);
					a_intfc->ReadRecordData(g_interest.data(), size);
				}
				Read(a_intfc, count);
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::FormID id = 0, resolved = 0;
					Child      c;
					if (!Read(a_intfc, id) || !Read(a_intfc, c.lastSeenDay) || !Read(a_intfc, c.lastMoodDay)) {
						return;
					}
					if (a_intfc->ResolveFormID(id, resolved)) {
						g_children[resolved] = c;
					}
				}
			}
			logger::info("State: loaded {} candidates, {} reactions, {} refusals", g_candidates.size(), g_reactions.size(), g_refusals.size());
		}

		void OnRevert(SKSE::SerializationInterface*)
		{
			g_candidates.clear();
			g_reactions.clear();
			g_refusals.clear();
			g_interest.clear();
			g_children.clear();
		}
	}

	Candidate& Of(RE::FormID a_candidate)
	{
		return g_candidates[a_candidate];
	}

	std::unordered_map<RE::FormID, Candidate>& All()
	{
		return g_candidates;
	}

	std::unordered_map<RE::FormID, Reaction>& Reactions()
	{
		return g_reactions;
	}

	std::unordered_map<RE::FormID, Refusal>& Refusals()
	{
		return g_refusals;
	}

	std::unordered_map<RE::FormID, Child>& Children()
	{
		return g_children;
	}

	std::string& PlayerInterest()
	{
		return g_interest;
	}

	float Today()
	{
		const auto calendar = RE::Calendar::GetSingleton();
		return calendar ? calendar->GetDaysPassed() : 0.0f;
	}

	void Register()
	{
		const auto s = SKSE::GetSerializationInterface();
		s->SetUniqueID(kId);
		s->SetSaveCallback(OnSave);
		s->SetLoadCallback(OnLoad);
		s->SetRevertCallback(OnRevert);
	}
}
