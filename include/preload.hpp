#include <Windows.h>
#include <stdexcept>
#include <safetyhook.hpp>
#include <spdlog/spdlog.h>


// BASS TYPES
typedef DWORD HSTREAM;
typedef unsigned __int64 QWORD;

// BASS FLAGS
#define BASS_SAMPLE_FLOAT 256
#define BASS_SAMPLE_MONO 2
#define BASS_SAMPLE_3D 8
#define BASS_SAMPLE_LOOP 4
#define BASS_STREAM_PRESCAN 0x20000
#define BASS_STREAM_AUTOFREE 0x40000
#define BASS_STREAM_DECODE 0x200000
#define BASS_ASYNCFILE 0x40000000
#define BASS_UNICODE 0x80000000



namespace as_scrobbler
{
    namespace basshook
    {



        SafetyHookInline g_scf_hook{};

        HSTREAM __fastcall BASS_StreamCreateFileHook(BOOL mem, const void* file, QWORD offset, QWORD length, DWORD flags)
        {
            spdlog::debug("Caught SCF function");
            if (flags & BASS_STREAM_PRESCAN) {
                spdlog::debug("Prescan ALREADY ENABLED.");
                return g_scf_hook.stdcall<HSTREAM>(mem, file, offset, length, flags);
            }
            else {
                spdlog::debug("Prescan DISABLED. Replacing.");
                return g_scf_hook.stdcall<HSTREAM>(mem, file, offset, length, flags);
            }
            
        }

        void init()
        {
            spdlog::info("Attaching bass.dll hooks");
            FARPROC scfaddr = GetProcAddress(GetModuleHandleA("bass.dll"), "BASS_StreamCreateFile");
            g_scf_hook = safetyhook::create_inline((void*)scfaddr, (void*)BASS_StreamCreateFileHook);
            if (!g_scf_hook)
            {
                spdlog::error("Bass hook at {0:p} failed.", fmt::ptr(&g_scf_hook));
                throw std::runtime_error("Bass hook failed");
            }
        }
    }
}