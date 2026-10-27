#include "Scenes.h"

#include "Script.h"
#include "State.h"

namespace Scenes
{
	using namespace IRF;

	namespace
	{
		// game thread (SKSE tasks)
		constexpr float kStale = 0.1f;  // game days (about 2 game hours): a scene still on by then is stuck
		constexpr float kBeatSeconds = 2.0f;

		Kind                         g_kind{ Kind::kNone };
		float                        g_since{ 0.0f };
		std::vector<RE::ActorHandle> g_cast;
		std::function<void()>        g_breakOff;
		std::function<void()>        g_beat;
		int                          g_beatId{ 0 };  // a beat from an earlier scene finds another number

		const char* Name(Kind a_kind)
		{
			switch (a_kind) {
			case Kind::kFamily:
				return "family conversation";
			case Kind::kConfess:
				return "confession";
			case Kind::kEvening:
				return "evening";
			case Kind::kFirstDate:
				return "first date";
			case Kind::kExpose:
				return "expose";
			case Kind::kStayHome:
				return "stay home";
			default:
				return "none";
			}
		}

		// the scene a moment's line belongs to
		Kind Owner(Moment a_moment)
		{
			switch (a_moment) {
			case Moment::kFamily:
				return Kind::kFamily;
			case Moment::kConfess:
				return Kind::kConfess;
			case Moment::kDateAfter:
				return Kind::kEvening;
			case Moment::kFirstDate:
				return Kind::kFirstDate;
			case Moment::kExposeAsk:
			case Moment::kExposeClaim:
			case Moment::kExposeCounter:
			case Moment::kExposeQuestion:
			case Moment::kExposeOutcome:
				return Kind::kExpose;
			case Moment::kStayHome:
				return Kind::kStayHome;
			default:
				return Kind::kNone;
			}
		}

		void Clear()
		{
			g_kind = Kind::kNone;
			g_cast.clear();
			g_breakOff = {};
			g_beat = {};
			++g_beatId;
		}

		void BeatLater()
		{
			Script::CallStatic("IRF_Internal"sv, "BeatLater"sv, static_cast<std::int32_t>(g_beatId), kBeatSeconds);
		}
	}

	bool Begin(Kind a_kind, std::initializer_list<RE::Actor*> a_cast, std::function<void()> a_breakOff, std::function<void()> a_beat)
	{
		if (g_kind == a_kind) {
			Clear();  // its module thinks it isn't on (an end it missed): it starts over
		} else if (g_kind != Kind::kNone) {
			const bool stale = State::Today() - g_since > kStale;  // its own checks missed the end: never block for good
			if (!stale && (g_kind > a_kind || !g_breakOff)) {
				logger::info("Scenes: no {} now, the {} has the stage", Name(a_kind), Name(g_kind));
				return false;
			}
			logger::info("Scenes: the {} breaks off the {}{}", Name(a_kind), Name(g_kind), stale ? " (stuck)" : "");
			const auto breakOff = std::move(g_breakOff);
			Clear();  // its End() finds nothing to end
			if (breakOff) {
				breakOff();
			}
		}
		g_kind = a_kind;
		g_since = State::Today();
		g_breakOff = std::move(a_breakOff);
		for (const auto actor : a_cast) {
			if (actor) {
				g_cast.push_back(actor->GetHandle());
			}
		}
		g_beat = std::move(a_beat);
		logger::info("Scenes: {} on", Name(a_kind));
		if (g_beat) {
			BeatLater();
		}
		return true;
	}

	void Settle(Kind a_kind)
	{
		if (g_kind == a_kind) {
			g_breakOff = {};
		}
	}

	void End(Kind a_kind)
	{
		if (g_kind == a_kind && a_kind != Kind::kNone) {
			logger::info("Scenes: {} over", Name(a_kind));
			Clear();
		}
	}

	bool On(Kind a_kind)
	{
		return g_kind == a_kind;
	}

	bool MaySay(RE::Actor* a_speaker, Moment a_moment)
	{
		if (g_kind == Kind::kNone || Owner(a_moment) == g_kind || a_moment == Moment::kAttacked || a_moment == Moment::kRevive ||
			State::Today() - g_since > kStale) {
			return true;  // a fight's lines always (a fight breaks any scene off anyway); a stuck scene keeps no one quiet
		}
		for (const auto& handle : g_cast) {
			if (handle.get().get() == a_speaker) {
				logger::info("Scenes: {} keeps her line for moment {} (in the {})", a_speaker->GetDisplayFullName(), Rank(a_moment), Name(g_kind));
				return false;
			}
		}
		return true;
	}

	void OnBeat(int a_beat)
	{
		if (a_beat != g_beatId || !g_beat) {
			return;  // that scene is over
		}
		const auto beat = g_beat;  // it may end the scene (and with it g_beat)
		beat();
		if (a_beat == g_beatId) {
			BeatLater();
		}
	}

	void OnGameLoaded()
	{
		Clear();  // a scene in a save isn't picked up again (each module clears its own)
	}
}
