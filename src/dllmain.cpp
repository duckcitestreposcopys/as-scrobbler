#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <chrono>
#include <thread>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>


#include "config.hpp"
#include "nethook.hpp"
#include "preload.hpp"

#include "scrobbler.hpp"

// Create a file rotating logger with 5 MB size max and 3 rotated files
auto max_size = 1048576 * 5;
auto max_log_files = 3;
auto logger = spdlog::rotating_logger_mt("preload_enabler", "logs/preload_enabler_log.txt", max_size, max_log_files);

uint32_t __stdcall init(void* args)
{
	spdlog::set_default_logger(logger);
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);
	spdlog::info("Init");

	try
	{
		as_scrobbler::config::init();
	}
	catch (const std::exception& e)
	{
		spdlog::error("Exception thrown when loading config: {}", e.what());
		MessageBoxA(nullptr, "Unable to load scrobble server replacement config", "Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	if (as_scrobbler::config::redirect_scrobbles) {

		auto scrobblerDll = LoadLibrary("audiosurfscrobblerlib.dll");
		FARPROC scrobbleAddress = GetProcAddress(scrobblerDll, "scrobble");
		if (scrobbleAddress == NULL) {
			spdlog::error("Unable to load scrobbler lib: {0}", GetLastError());
			MessageBoxA(nullptr, "Unable to load scrobbler library", "Error", MB_OK | MB_ICONERROR);
			return 1;
		}
		auto scrobbleFunc = reinterpret_cast<scrobble>(scrobbleAddress);

		while (!GetModuleHandleA("HTTP_Fetch_Unicode.dll") || !GetModuleHandleA("17C5B19F-4273-423C-A158-CA6F73046D43.dll"))
			Sleep(100);

		try
		{
			as_scrobbler::nethook::init(scrobbleFunc);
		}
		catch (const std::exception& e)
		{
			MessageBoxA(nullptr, "Scrobble server hook failed", "Error", MB_OK | MB_ICONERROR);
			return 1;
		}
	}
	else {
		spdlog::info("Not redirecting scrobbles.");
	}


	if (as_scrobbler::config::enable_preload) {
		while (!GetModuleHandleA("bass.dll")) {
			Sleep(100);
		}

		spdlog::info("bass loaded");

		try {
			as_scrobbler::basshook::init();

		}
		catch (const std::exception& e) {

		}
	}
	else {
		spdlog::info("Not enabling preload.");
	}


	spdlog::info("Done");

	while (true)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

	return 0;
}

BOOL WINAPI DllMain(HMODULE handle, DWORD reason, LPVOID reserved)
{	


	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(handle);

		if (const auto thread = (HANDLE)_beginthreadex(nullptr, 0, &init, nullptr, 0, nullptr))
		{
			CloseHandle(thread);
			return 1;
		}
	}

	return 0;
}
