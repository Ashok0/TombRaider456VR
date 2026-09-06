// proxytest.cpp -- proves the winmm proxy forwards to the real winmm instead of
// to itself.
//
// The failure this is written to catch: the Windows loader keys loaded modules
// partly by base name, so a DLL called winmm.dll asking LoadLibrary for
// System32\winmm.dll could plausibly be handed back its own handle. If that
// happened, GetProcAddress("timeGetTime") would return the proxy's own thunk and
// the first call would recurse until the stack died.
//
// So: load the proxy by absolute path, call through it, and check both that the
// answer is sane and that two physically distinct winmm modules are mapped.

#include <windows.h>
#include <cstdio>

typedef DWORD (WINAPI* PFN_timeGetTime)(void);
typedef UINT  (WINAPI* PFN_timeBeginPeriod)(UINT);

extern "C" __declspec(dllimport) DWORD WINAPI K32EnumProcessModules(
    HANDLE, HMODULE*, DWORD, LPDWORD);
extern "C" __declspec(dllimport) DWORD WINAPI K32GetModuleFileNameExW(
    HANDLE, HMODULE, LPWSTR, DWORD);

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_fail;
}

static int CountLoadedWinmm() {
    HMODULE mods[1024];
    DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) return -1;
    const int n = static_cast<int>(needed / sizeof(HMODULE));
    int count = 0;
    for (int i = 0; i < n; ++i) {
        wchar_t path[MAX_PATH]{};
        if (!K32GetModuleFileNameExW(GetCurrentProcess(), mods[i], path, MAX_PATH)) continue;
        const wchar_t* base = wcsrchr(path, L'\\');
        base = base ? base + 1 : path;
        if (_wcsicmp(base, L"winmm.dll") == 0) {
            ++count;
            wprintf(L"      winmm #%d: %s\n", count, path);
        }
    }
    return count;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        printf("usage: proxytest <dir containing the proxy winmm.dll>\n");
        return 2;
    }

    wchar_t proxyPath[MAX_PATH]{};
    swprintf_s(proxyPath, L"%s\\winmm.dll", argv[1]);

    printf("winmm proxy test\n================\n\n");
    wprintf(L"proxy: %s\n\n", proxyPath);

    printf("before load:\n");
    CountLoadedWinmm();

    HMODULE proxy = LoadLibraryW(proxyPath);
    Check(proxy != nullptr, "proxy winmm.dll loads");
    if (!proxy) {
        printf("  GetLastError = %lu\n", GetLastError());
        return 1;
    }

    auto timeGetTimeFn = reinterpret_cast<PFN_timeGetTime>(
        GetProcAddress(proxy, "timeGetTime"));
    Check(timeGetTimeFn != nullptr, "timeGetTime is exported");
    if (!timeGetTimeFn) return 1;

    // If forwarding were circular this call would never return.
    const DWORD t1   = timeGetTimeFn();
    const DWORD tick = GetTickCount();
    Check(t1 != 0, "timeGetTime returns non-zero (no self-recursion)");

    // timeGetTime and GetTickCount both count ms since boot, so they should be
    // within a whisker of each other. A wrong forward would not be.
    const DWORD delta = (t1 > tick) ? (t1 - tick) : (tick - t1);
    printf("      timeGetTime=%lu  GetTickCount=%lu  delta=%lu ms\n", t1, tick, delta);
    Check(delta < 250, "timeGetTime agrees with GetTickCount (real winmm reached)");

    Sleep(60);
    const DWORD t2 = timeGetTimeFn();
    printf("      after 60ms sleep: %lu (advanced %lu)\n", t2, t2 - t1);
    Check(t2 > t1, "clock advances across calls");

    auto beginPeriod = reinterpret_cast<PFN_timeBeginPeriod>(
        GetProcAddress(proxy, "timeBeginPeriod"));
    Check(beginPeriod != nullptr, "timeBeginPeriod is exported");
    if (beginPeriod) {
        const UINT r = beginPeriod(1);
        printf("      timeBeginPeriod(1) -> %u (0 == TIMERR_NOERROR)\n", r);
        Check(r == 0, "timeBeginPeriod succeeds through the proxy");
    }

    printf("\nafter load:\n");
    const int winmmCount = CountLoadedWinmm();
    Check(winmmCount >= 2,
          "two distinct winmm.dll modules mapped (proxy + real, no base-name collision)");

    const bool modLoaded = GetModuleHandleW(L"TombRaiderVR.dll") != nullptr;
    printf("\n  TombRaiderVR.dll loaded by proxy: %s\n", modLoaded ? "yes" : "no");
    Check(modLoaded, "proxy side-loaded TombRaiderVR.dll");

    printf("\n%s (%d failure%s)\n",
           g_fail == 0 ? "ALL PASSED" : "FAILURES",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
