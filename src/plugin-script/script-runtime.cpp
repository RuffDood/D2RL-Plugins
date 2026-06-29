#include "script-runtime.h"
#include "script-types.h"
#include "script-bindings.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <string>

// ── Global state ──────────────────────────────────────────────────────────────

JSRuntime*                   g_rt      = nullptr;
JSContext*                   g_ctx     = nullptr;
uintptr_t                    g_ExeBase = 0;
const D2RLoaderPluginContext* g_Context = nullptr;

// ── Runtime init/shutdown ─────────────────────────────────────────────────────

bool Script_RuntimeInit(const ScriptPluginOptions& opts) {
    g_rt = JS_NewRuntime();
    if (!g_rt) return false;

    JS_SetMemoryLimit(g_rt, static_cast<size_t>(opts.memoryLimitMB) * 1024 * 1024);
    JS_SetMaxStackSize(g_rt, static_cast<size_t>(opts.stackSizeKB) * 1024);

    g_ctx = JS_NewContext(g_rt);
    if (!g_ctx) {
        JS_FreeRuntime(g_rt);
        g_rt = nullptr;
        return false;
    }

    Script_RegisterGameTypes(g_ctx);
    Script_RegisterTrapObject(g_ctx);
    return true;
}

void Script_RuntimeShutdown() {
    if (g_ctx) { JS_FreeContext(g_ctx); g_ctx = nullptr; }
    if (g_rt)  { JS_FreeRuntime(g_rt);  g_rt  = nullptr; }
}

// ── Script loading ────────────────────────────────────────────────────────────

static void LogException(const char* filename, const D2RLoaderPluginContext* context) {
    JSValue exc = JS_GetException(g_ctx);
    JSValue str = JS_ToString(g_ctx, exc);
    const char* msg = JS_ToCString(g_ctx, str);
    if (msg)
        D2RPluginLogErrorF(context, "plugin-script: error in %s: %s", filename, msg);
    JS_FreeCString(g_ctx, msg);
    JS_FreeValue(g_ctx, str);
    JS_FreeValue(g_ctx, exc);
}

static void LoadOneScript(const std::string& path, const D2RLoaderPluginContext* context) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        D2RPluginLogErrorF(context, "plugin-script: cannot open %s", path.c_str());
        return;
    }

    auto size = static_cast<std::streamsize>(f.tellg());
    f.seekg(0);
    std::string src(static_cast<size_t>(size), '\0');
    if (!f.read(src.data(), size)) {
        D2RPluginLogErrorF(context, "plugin-script: cannot read %s", path.c_str());
        return;
    }

    // Extract just the filename portion for error messages.
    const char* displayName = path.c_str();
    for (const char* p = displayName; *p; ++p)
        if (*p == '/' || *p == '\\') displayName = p + 1;

    JSValue result = JS_Eval(g_ctx, src.c_str(), src.size(), displayName, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        LogException(displayName, context);
    } else {
        D2RPluginLogInfoF(context, "plugin-script: loaded %s", displayName);
    }
    JS_FreeValue(g_ctx, result);
}

void Script_LoadScripts(const std::string& directory, const D2RLoaderPluginContext* context) {
    std::string pattern = directory + "\\*.js";

    // Widen pattern for FindFirstFileW.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;
    std::wstring wpattern(static_cast<size_t>(wlen - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, wpattern.data(), wlen);

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(wpattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        // Convert found filename to UTF-8 and build full path.
        int fnLen = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nullptr, 0, nullptr, nullptr);
        if (fnLen <= 0) continue;
        std::string filename(static_cast<size_t>(fnLen - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, filename.data(), fnLen, nullptr, nullptr);

        LoadOneScript(directory + "\\" + filename, context);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}
