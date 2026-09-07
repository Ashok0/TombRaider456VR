#include "Config.h"
#include "Log.h"

#include <windows.h>
#include <cstdlib>
#include <cstdio>
#include <cwchar>

namespace tr {
namespace {

Config g_cfg;
float  g_liveScale = 423.0f;
float  g_liveIpd   = 1.0f;

int GetInt(const wchar_t* key, int def, const wchar_t* ini) {
    return static_cast<int>(GetPrivateProfileIntW(L"VR", key, def, ini));
}

bool GetBool(const wchar_t* key, bool def, const wchar_t* ini) {
    return GetInt(key, def ? 1 : 0, ini) != 0;
}

// GetPrivateProfileIntW parses decimal only and stops at the first non-digit,
// so "0x79" would come back as 0 and silently disable the feature. Read the raw
// string and let wcstol's base-0 handle 0x/decimal alike.
int GetIntAuto(const wchar_t* key, int def, const wchar_t* ini) {
    wchar_t buf[64] = {};
    wchar_t defBuf[64] = {};
    swprintf_s(defBuf, L"%d", def);
    GetPrivateProfileStringW(L"VR", key, defBuf, buf, 64, ini);
    wchar_t* end = nullptr;
    const long v = wcstol(buf, &end, 0);
    if (end == buf) return def;
    return static_cast<int>(v);
}

float GetFloat(const wchar_t* key, float def, const wchar_t* ini) {
    wchar_t buf[64] = {};
    wchar_t defBuf[64] = {};
    swprintf_s(defBuf, L"%g", def);
    GetPrivateProfileStringW(L"VR", key, defBuf, buf, 64, ini);
    return static_cast<float>(_wtof(buf));
}

} // namespace

const Config& Cfg() { return g_cfg; }

float LiveWorldUnitsPerMetre() { return g_liveScale; }
float LiveIpdScale()           { return g_liveIpd; }

void AdjustWorldScale(float factor) {
    g_liveScale *= factor;
    if (g_liveScale < 16.0f)   g_liveScale = 16.0f;
    if (g_liveScale > 8192.0f) g_liveScale = 8192.0f;
    LogTuning("world scale");
}

void AdjustIpdScale(float factor) {
    g_liveIpd *= factor;
    if (g_liveIpd < 0.1f)  g_liveIpd = 0.1f;
    if (g_liveIpd > 10.0f) g_liveIpd = 10.0f;
    LogTuning("ipd");
}

void ResetTuning() {
    g_liveScale = g_cfg.worldUnitsPerMetre;
    g_liveIpd   = g_cfg.ipdScale;
    LogTuning("reset to ini");
}

void WarnIgnoredOptions() {
    // Mode 3 never writes the view matrix, so anything that acts on it is inert.
    if (g_cfg.eyeOffsetMode == 3) {
        if (!g_cfg.perEyeView) {
            Log("config: PerEyeView=0 is IGNORED with EyeOffsetMode=3 -- that "
                "mode never writes the view matrix. Set EyeOffsetMode=0 to use it.");
        }
        if (g_cfg.debugEyeYawDegrees != 0.0f) {
            Log("config: DebugEyeYawDegrees is IGNORED with EyeOffsetMode=3 -- "
                "it acts on the view matrix, which that mode leaves untouched.");
        }
    }
}

void LogTuning(const char* why) {
    LogF("tuning [%s]: WorldUnitsPerMetre=%.1f  IpdScale=%.3f "
         "(eye separation ~%.1f world units for a 64 mm IPD)",
         why, g_liveScale, g_liveIpd, 0.064f * g_liveScale * g_liveIpd);
}

void LoadConfig(const wchar_t* ini) {
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
        Log("config: no TombRaiderVR.ini found, using defaults");
        return;
    }

    g_cfg.enabled             = GetBool (L"Enabled",            g_cfg.enabled,            ini);

    // Mode=mono | stereo
    {
        wchar_t mode[32] = {};
        GetPrivateProfileStringW(L"VR", L"Mode", L"stereo", mode, 32, ini);
        g_cfg.monoTracking = (_wcsicmp(mode, L"mono") == 0);
    }

    g_cfg.positionalTracking  = GetBool (L"PositionalTracking", g_cfg.positionalTracking, ini);
    g_cfg.seatedOrigin        = GetBool (L"SeatedOrigin",       g_cfg.seatedOrigin,       ini);
    g_cfg.worldUnitsPerMetre  = GetFloat(L"WorldUnitsPerMetre", g_cfg.worldUnitsPerMetre, ini);
    g_cfg.ipdScale            = GetFloat(L"IpdScale",           g_cfg.ipdScale,           ini);
    g_cfg.scaleUpKey          = GetIntAuto(L"ScaleUpKey",       g_cfg.scaleUpKey,         ini);
    g_cfg.scaleDownKey        = GetIntAuto(L"ScaleDownKey",     g_cfg.scaleDownKey,       ini);
    g_cfg.ipdUpKey            = GetIntAuto(L"IpdUpKey",         g_cfg.ipdUpKey,           ini);
    g_cfg.ipdDownKey          = GetIntAuto(L"IpdDownKey",       g_cfg.ipdDownKey,         ini);
    g_cfg.resetTuningKey      = GetIntAuto(L"ResetTuningKey",   g_cfg.resetTuningKey,     ini);
    g_cfg.scaleStep           = GetFloat(L"ScaleStep",          g_cfg.scaleStep,          ini);
    if (g_cfg.scaleStep < 1.01f) g_cfg.scaleStep = 1.01f;
    if (g_cfg.scaleStep > 4.0f)  g_cfg.scaleStep = 4.0f;
    g_cfg.eyeWidth            = GetInt  (L"EyeWidth",           g_cfg.eyeWidth,           ini);
    g_cfg.eyeHeight           = GetInt  (L"EyeHeight",          g_cfg.eyeHeight,          ini);
    g_cfg.superSample         = GetFloat(L"SuperSample",        g_cfg.superSample,        ini);
    g_cfg.flipProjectionY     = GetBool (L"FlipProjectionY",    g_cfg.flipProjectionY,    ini);
    g_cfg.flipViewY           = GetBool (L"FlipViewY",          g_cfg.flipViewY,          ini);
    g_cfg.swapEyes            = GetBool (L"SwapEyes",           g_cfg.swapEyes,           ini);
    g_cfg.flipSubmitV         = GetBool (L"FlipSubmitV",        g_cfg.flipSubmitV,        ini);
    g_cfg.eyeMarkers          = GetBool (L"EyeMarkers",         g_cfg.eyeMarkers,         ini);
    g_cfg.perEyeView          = GetBool (L"PerEyeView",         g_cfg.perEyeView,         ini);
    g_cfg.debugEyeYawDegrees  = GetFloat(L"DebugEyeYawDegrees", g_cfg.debugEyeYawDegrees, ini);
    g_cfg.eyeOffsetMode       = GetIntAuto(L"EyeOffsetMode",    g_cfg.eyeOffsetMode,      ini);
    g_cfg.hudDepthMetres      = GetFloat(L"HudDepthMetres",     g_cfg.hudDepthMetres,     ini);
    g_cfg.hudSizeDegrees      = GetFloat(L"HudSizeDegrees",     g_cfg.hudSizeDegrees,     ini);
    g_cfg.hudLockToHead       = GetBool (L"HudLockToHead",      g_cfg.hudLockToHead,      ini);
    g_cfg.hudFlipY            = GetBool (L"HudFlipY",           g_cfg.hudFlipY,           ini);
    g_cfg.videoDepthMetres    = GetFloat(L"VideoDepthMetres",   g_cfg.videoDepthMetres,   ini);
    g_cfg.videoLockToHead     = GetBool (L"VideoLockToHead",    g_cfg.videoLockToHead,    ini);
    g_cfg.videoSizeDegrees    = GetFloat(L"VideoSizeDegrees",   g_cfg.videoSizeDegrees,   ini);
    g_cfg.videoOffscreen      = GetBool (L"VideoOffscreen",     g_cfg.videoOffscreen,     ini);
    g_cfg.videoFlipV          = GetBool (L"VideoFlipV",         g_cfg.videoFlipV,         ini);
    g_cfg.videoSkipGame6      = GetBool (L"VideoSkipGame6",     g_cfg.videoSkipGame6,     ini);
    g_cfg.alternateEyeGame6   = GetBool (L"AlternateEyeGame6",  g_cfg.alternateEyeGame6,  ini);
    g_cfg.alternateEyeMinOffscreen =
        GetIntAuto(L"AlternateEyeMinOffscreen", g_cfg.alternateEyeMinOffscreen, ini);
    g_cfg.gamepadEnabled      = GetBool (L"GamepadEnabled",     g_cfg.gamepadEnabled,     ini);
    g_cfg.gamepadLogButtons   = GetBool (L"GamepadLogButtons",  g_cfg.gamepadLogButtons,  ini);
    g_cfg.gamepadMenuUsesBack = GetBool (L"GamepadMenuUsesBack", g_cfg.gamepadMenuUsesBack, ini);
    g_cfg.perEyeProjection    = GetIntAuto(L"PerEyeProjection", g_cfg.perEyeProjection,   ini);
    g_cfg.flatHud             = GetBool (L"FlatHud",            g_cfg.flatHud,            ini);
    g_cfg.duplicateDraws      = GetBool (L"DuplicateDraws",     g_cfg.duplicateDraws,     ini);
    g_cfg.mirrorToWindow      = GetBool (L"MirrorToWindow",     g_cfg.mirrorToWindow,     ini);
    g_cfg.nearClip            = GetFloat(L"NearClip",           g_cfg.nearClip,           ini);
    g_cfg.farClip             = GetFloat(L"FarClip",            g_cfg.farClip,            ini);
    g_cfg.verboseFirstFrame   = GetBool (L"VerboseFirstFrame",  g_cfg.verboseFirstFrame,  ini);
    g_cfg.traceFrames         = GetInt  (L"TraceFrames",        g_cfg.traceFrames,        ini);
    g_cfg.traceStartFrame     = GetInt  (L"TraceStartFrame",    g_cfg.traceStartFrame,    ini);
    g_cfg.traceKey            = GetIntAuto(L"TraceKey",         g_cfg.traceKey,           ini);

    g_liveScale = g_cfg.worldUnitsPerMetre;
    g_liveIpd   = g_cfg.ipdScale;

    if (g_cfg.superSample < 0.25f) g_cfg.superSample = 0.25f;
    if (g_cfg.superSample > 4.0f)  g_cfg.superSample = 4.0f;
    if (g_cfg.worldUnitsPerMetre < 1.0f) g_cfg.worldUnitsPerMetre = 1.0f;

    LogF("config: mode=%s tracking=%s origin=%s units/m=%.1f ss=%.2f "
         "flipProjY=%d flipViewY=%d dup=%d flatHud=%d",
         g_cfg.monoTracking ? "MONO head-tracking" : "stereo",
         g_cfg.positionalTracking ? "rotation+position" : "rotation only",
         g_cfg.seatedOrigin ? "seated" : "standing",
         g_cfg.worldUnitsPerMetre, g_cfg.superSample,
         g_cfg.flipProjectionY, g_cfg.flipViewY,
         g_cfg.duplicateDraws, g_cfg.flatHud);
    if (g_cfg.traceFrames > 0) {
        LogF("config: frame-graph trace armed -- %d frame(s), hotkey vk=0x%02X",
             g_cfg.traceFrames, g_cfg.traceKey);
    }
}

} // namespace tr
