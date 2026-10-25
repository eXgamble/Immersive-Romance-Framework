// Immersive Romance Framework: SKSE plugin.
// Courting, dating and commitment before vanilla's marriage, with approval in between. Candidates come from
// add-ons (a JSON manifest each); the framework keeps what lines read in faction ranks on the candidate.

#include "Candidates.h"
#include "Crimes.h"
#include "Events.h"
#include "Follower.h"
#include "Forms.h"
#include "Papyrus.h"
#include "Romance.h"
#include "Rules.h"
#include "State.h"

namespace
{
	bool g_ready = false;
	bool g_toldRejected = false;

	// Own the log before SKSE::Init, and pass Init false so it doesn't truncate/replace it.
	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Failed to find the SKSE log directory"sv);
		}
		*path /= "ImmersiveRomanceFramework.log"sv;

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S:%e] [%l] %v"s);
	}

	// One notification per session naming the add-on candidates that were not registered (details in the log)
	void TellRejected()
	{
		if (g_toldRejected || Candidates::Rejected().empty()) {
			return;
		}
		g_toldRejected = true;
		RE::SendHUDMessage::ShowHUDMessage(std::format("Immersive Romance Framework: {} candidate(s) not registered, see ImmersiveRomanceFramework.log",
			Candidates::Rejected().size())
											   .c_str());
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			g_ready = Forms::Load();
			if (g_ready) {
				Rules::Load();
				Candidates::Load();
				Events::Register();
				Crimes::Install();
			}
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			if (g_ready) {
				Follower::OnGameLoaded();
				Events::OnGameLoaded();
				Crimes::OnGameLoaded();
			}
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			if (g_ready) {
				SKSE::GetTaskInterface()->AddTask([]() {
					Romance::StartQuests();
					Romance::SyncAll();
					TellRejected();
				});
			}
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	logger::info("ImmersiveRomanceFramework loading on game runtime {}", a_skse->RuntimeVersion().string("."));

	SKSE::Init(a_skse, false);
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	SKSE::GetPapyrusInterface()->Register(Papyrus::Register);
	State::Register();

	logger::info("ImmersiveRomanceFramework loaded");
	return true;
}
