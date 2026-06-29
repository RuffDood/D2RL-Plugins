#pragma once

#include "script-private.h"
#include "script-config.h"

// Initialize the QuickJS runtime and context, register all game types and the
// trap object. Returns false on failure.
bool Script_RuntimeInit(const ScriptPluginOptions& opts);

// Enumerate and execute all .js files in the given directory.
// Calls trap.set* registrations inside each script, populating g_Registry.
void Script_LoadScripts(const std::string& directory, const D2RLoaderPluginContext* context);

// Free the QuickJS context and runtime.
void Script_RuntimeShutdown();
