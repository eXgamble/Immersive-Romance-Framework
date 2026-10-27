#include "State.h"

// The co-save, built to outlive updates (format 2, 2026-10-27). Every saved value is a tagged field: a four-letter
// tag, its size, its bytes. A load reads the tags it knows and steps over the rest, and a field the save lacks keeps
// its default, so an update may add a field (a new tag) or stop saving one (its tag is skipped) and old saves still
// load. One record per kind ('CAND', 'REAC', 'REFU', 'CHLD', 'PLYR'); a record kind this build doesn't know is skipped,
// so a new kind is safe too.
// The contract, once released: a tag is never reused and never given another type or meaning (that's a new tag);
// enums are saved by value (Types.h: never renumber). Bumping kFormat drops every older save's state: a major version.
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
		constexpr std::uint32_t kFormat = 2;
		constexpr std::uint32_t kOldRecord = 'IRFS';  // format 1 (fixed layout, before 2026-10-27): not read
		constexpr std::uint32_t kCandidates = 'CAND';
		constexpr std::uint32_t kReactions = 'REAC';
		constexpr std::uint32_t kRefusals = 'REFU';
		constexpr std::uint32_t kChildren = 'CHLD';
		constexpr std::uint32_t kPlayer = 'PLYR';
		constexpr std::uint32_t kEnd = 0;  // after an entry's last field

		struct Player {};  // the player's own values (g_interest)

		// ---- the fields: one list, written and read through it (a visitor gets each tag and value)

		template <class V>
		void Fields(V& v, Candidate& c)
		{
			v('cont', c.lastContactDay);
			v('slnt', c.silentUntilDay);
			v('hurt', c.hurt);
			v('why_', c.why);
			v('who_', c.who);
			v('brkd', c.breakupDay);
			v('nfrg', c.unforgivable);
			v('scrt', c.secret);
			v('lock', c.lockUntilDay);
			v('date', c.lastDateDay);
			v('kiss', c.lastKissDay);
			v('gift', c.lastGiftDay);
			v('kild', c.killDay);
			v('kils', c.killsToday);
			v('rcnt', c.recent);
			v('rcnd', c.recentDay);
			v('pxp_', c.partnerXp);
			v('plvl', c.appliedLevels);
			v('prof', c.profileApplied);
			v('conf', c.savedConfidence);
			v('aggr', c.savedAggression);
			v('pbrk', c.pendingBreakup);
			v('know', c.knowSeeing);
			v('saws', c.sawStraying);
			v('cfrp', c.confrontPending);
			v('cfrw', c.confrontWorks);
			v('wary', c.waryUntilDay);
			v('jeal', c.jealousUntilDay);
			v('plds', c.pleadsLeft);
			v('pldd', c.nextPleadDay);
			v('redy', c.readyDay);
			v('dtng', c.datingDay);
			v('cnfs', c.confessed);
			v('askd', c.knowAsked);
			v('askl', c.lastKnowDay);
			v('dtun', c.dateUntilDay);
			v('dted', c.dated);
			// strayHits: the current fight only, never saved
		}

		template <class V>
		void Fields(V& v, Reaction& r)
		{
			v('evnt', r.event);
			v('untl', r.untilDay);
		}

		template <class V>
		void Fields(V& v, Refusal& r)
		{
			v.Form('bcse', r.because);
			v('untl', r.untilDay);
		}

		template <class V>
		void Fields(V& v, Child& c)
		{
			v('seen', c.lastSeenDay);
			v('mood', c.lastMoodDay);
		}

		template <class V>
		void Fields(V& v, Player&)
		{
			v('intr', g_interest);
		}

		// ---- raw values

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

		bool Skip(SKSE::SerializationInterface* a_intfc, std::uint32_t a_size)
		{
			std::array<std::byte, 256> scratch;
			while (a_size > 0) {
				const auto chunk = (std::min)(a_size, static_cast<std::uint32_t>(scratch.size()));
				if (a_intfc->ReadRecordData(scratch.data(), chunk) != chunk) {
					return false;
				}
				a_size -= chunk;
			}
			return true;
		}

		std::string TagName(std::uint32_t a_tag)
		{
			std::string out;
			for (int shift = 24; shift >= 0; shift -= 8) {
				out += static_cast<char>((a_tag >> shift) & 0xFF);
			}
			return out;
		}

		// ---- visitors

		struct Writer
		{
			SKSE::SerializationInterface* intfc;

			void Head(std::uint32_t a_tag, std::uint32_t a_size)
			{
				Write(intfc, a_tag);
				Write(intfc, a_size);
			}

			template <class T>
			void operator()(std::uint32_t a_tag, const T& a_value)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				Head(a_tag, sizeof(T));
				Write(intfc, a_value);
			}

			void operator()(std::uint32_t a_tag, const std::set<RE::FormID>& a_set)
			{
				Head(a_tag, static_cast<std::uint32_t>(a_set.size() * sizeof(RE::FormID)));
				for (const auto id : a_set) {
					Write(intfc, id);
				}
			}

			void operator()(std::uint32_t a_tag, const std::string& a_text)
			{
				Head(a_tag, static_cast<std::uint32_t>(a_text.size()));
				intfc->WriteRecordData(a_text.data(), static_cast<std::uint32_t>(a_text.size()));
			}

			void Form(std::uint32_t a_tag, RE::FormID a_id) { (*this)(a_tag, a_id); }
		};

		// Reads the one field whose tag came next; the rest of the list passes it by
		struct Reader
		{
			SKSE::SerializationInterface* intfc;
			std::uint32_t                 tag;
			std::uint32_t                 size;
			bool                          taken{ false };   // a field read it
			bool                          broken{ false };  // the record ended inside it

			bool Mine(std::uint32_t a_tag) const { return !taken && a_tag == tag; }

			template <class T>
			void operator()(std::uint32_t a_tag, T& a_value)
			{
				if (Mine(a_tag) && size == sizeof(T)) {  // another size isn't this field's type: skipped
					taken = true;
					broken = !Read(intfc, a_value);
				}
			}

			void operator()(std::uint32_t a_tag, std::set<RE::FormID>& a_set)
			{
				if (!Mine(a_tag) || size % sizeof(RE::FormID) != 0) {
					return;
				}
				taken = true;
				for (std::uint32_t i = 0; i < size / sizeof(RE::FormID); ++i) {
					RE::FormID id = 0, resolved = 0;
					if (!Read(intfc, id)) {
						broken = true;
						return;
					}
					if (intfc->ResolveFormID(id, resolved)) {  // its plugin may be gone
						a_set.insert(resolved);
					}
				}
			}

			void operator()(std::uint32_t a_tag, std::string& a_text)
			{
				if (Mine(a_tag) && size < 256) {
					taken = true;
					a_text.resize(size);
					broken = intfc->ReadRecordData(a_text.data(), size) != size;
				}
			}

			void Form(std::uint32_t a_tag, RE::FormID& a_id)
			{
				const bool mine = Mine(a_tag);
				(*this)(a_tag, a_id);
				if (mine && taken && !broken && !intfc->ResolveFormID(a_id, a_id)) {
					a_id = 0;  // its plugin is gone
				}
			}
		};

		// Lists every tag, for the duplicate check at startup
		struct Tags
		{
			std::vector<std::uint32_t> seen;

			template <class T>
			void operator()(std::uint32_t a_tag, const T&)
			{
				seen.push_back(a_tag);
			}

			void Form(std::uint32_t a_tag, RE::FormID) { seen.push_back(a_tag); }
		};

		template <class T>
		void CheckTags(const char* a_kind)
		{
			T    entry{};
			Tags tags;
			Fields(tags, entry);
			std::ranges::sort(tags.seen);
			for (std::size_t i = 1; i < tags.seen.size(); ++i) {
				if (tags.seen[i] == tags.seen[i - 1]) {
					logger::critical("State: two {} fields share the tag '{}' (one of them never loads)", a_kind, TagName(tags.seen[i]));
				}
			}
		}

		// ---- entries and records

		template <class T>
		void WriteFields(SKSE::SerializationInterface* a_intfc, T& a_entry)
		{
			Writer w{ a_intfc };
			Fields(w, a_entry);
			Write(a_intfc, kEnd);
		}

		template <class T>
		bool ReadFields(SKSE::SerializationInterface* a_intfc, T& a_entry)
		{
			for (;;) {
				std::uint32_t tag = 0, size = 0;
				if (!Read(a_intfc, tag)) {
					return false;
				}
				if (tag == kEnd) {
					return true;
				}
				if (!Read(a_intfc, size)) {
					return false;
				}
				Reader r{ a_intfc, tag, size };
				Fields(r, a_entry);
				if (r.broken || (!r.taken && !Skip(a_intfc, size))) {
					return false;
				}
			}
		}

		template <class T>
		void WriteMap(SKSE::SerializationInterface* a_intfc, std::uint32_t a_record, std::unordered_map<RE::FormID, T>& a_map)
		{
			if (!a_intfc->OpenRecord(a_record, kFormat)) {
				logger::error("State: could not open the co-save record '{}'", TagName(a_record));
				return;
			}
			Write(a_intfc, static_cast<std::uint32_t>(a_map.size()));
			for (auto& [id, entry] : a_map) {
				Write(a_intfc, id);
				WriteFields(a_intfc, entry);
			}
		}

		template <class T>
		void ReadMap(SKSE::SerializationInterface* a_intfc, std::uint32_t a_record, std::unordered_map<RE::FormID, T>& a_map)
		{
			std::uint32_t count = 0;
			if (!Read(a_intfc, count)) {
				return;
			}
			for (std::uint32_t i = 0; i < count; ++i) {
				RE::FormID id = 0, resolved = 0;
				T          entry{};
				if (!Read(a_intfc, id) || !ReadFields(a_intfc, entry)) {
					logger::error("State: co-save record '{}' cut short after {} of {}", TagName(a_record), i, count);
					return;
				}
				if (a_intfc->ResolveFormID(id, resolved)) {  // gone with its plugin: dropped
					a_map[resolved] = std::move(entry);
				}
			}
		}

		void OnSave(SKSE::SerializationInterface* a_intfc)
		{
			WriteMap(a_intfc, kCandidates, g_candidates);
			WriteMap(a_intfc, kReactions, g_reactions);
			WriteMap(a_intfc, kRefusals, g_refusals);
			WriteMap(a_intfc, kChildren, g_children);
			if (a_intfc->OpenRecord(kPlayer, kFormat)) {
				Player player;
				WriteFields(a_intfc, player);
			}
		}

		void OnLoad(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t type = 0, version = 0, length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (type == kOldRecord) {
					logger::warn("State: this save is from before co-save format 2 (a development build); its romance state isn't read");
					continue;
				}
				if (version != kFormat) {
					logger::warn("State: co-save record '{}' has format {}, this framework reads {}: skipped", TagName(type), version, kFormat);
					continue;
				}
				switch (type) {
				case kCandidates:
					ReadMap(a_intfc, type, g_candidates);
					break;
				case kReactions:
					ReadMap(a_intfc, type, g_reactions);
					break;
				case kRefusals:
					ReadMap(a_intfc, type, g_refusals);
					std::erase_if(g_refusals, [](const auto& a_entry) { return a_entry.second.because == 0; });  // its candidate is gone
					break;
				case kChildren:
					ReadMap(a_intfc, type, g_children);
					break;
				case kPlayer:
					{
						Player player;
						if (!ReadFields(a_intfc, player)) {
							logger::error("State: co-save record 'PLYR' cut short");
						}
						break;
					}
				default:
					break;  // a kind from a newer framework: not ours to read
				}
			}
			logger::info("State: loaded {} candidates, {} reactions, {} refusals, {} children", g_candidates.size(), g_reactions.size(),
				g_refusals.size(), g_children.size());
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
		CheckTags<Candidate>("candidate");
		CheckTags<Reaction>("reaction");
		CheckTags<Refusal>("refusal");
		CheckTags<Child>("child");
		CheckTags<Player>("player");
		const auto s = SKSE::GetSerializationInterface();
		s->SetUniqueID(kId);
		s->SetSaveCallback(OnSave);
		s->SetLoadCallback(OnLoad);
		s->SetRevertCallback(OnRevert);
	}
}
