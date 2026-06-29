#pragma once

#include <plugin-shared.h>
#include <plugin-shared-json.h>
#include <quickjs.h>

// Global QuickJS state — owned by script-runtime.cpp, used throughout.
extern JSRuntime* g_rt;
extern JSContext* g_ctx;

// Cached at LoadHooks time.
extern uintptr_t                     g_ExeBase;
extern const D2RLoaderPluginContext* g_Context;
