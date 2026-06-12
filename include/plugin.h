#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdint>

#define D2RLOADER_PLUGIN_API_VERSION 1u
#define D2RLOADER_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#define D2RLOADER_PLUGIN_GET_INFO_EXPORT "D2RLoaderGetPluginInfo"
#define D2RLOADER_PLUGIN_LOAD_HOOKS_EXPORT "D2RLoaderLoadHooks"
#define D2RLOADER_PLUGIN_UNLOAD_EXPORT "D2RLoaderUnload"

enum D2RLoaderPluginFlags : uint32_t {
	D2RLoaderPluginFlag_None          = 0,
	D2RLoaderPluginFlag_ModScopedOnly = 1u << 0,
};

enum D2RLoaderPluginLoadScope : uint32_t {
	D2RLoaderPluginLoadScope_Mod    = 1,
	D2RLoaderPluginLoadScope_Global = 2,
};

struct D2RLoaderPluginInfo {
	uint32_t apiVersion;
	const char* id;
	const char* name;
	const char* version;
	const char* author;
	uint32_t flags;
};

struct D2RLoaderPluginContext {
	uint32_t apiVersion;
	D2RLoaderPluginLoadScope loadScope;
	uintptr_t exeBase;
	const char* activeMod;
	const wchar_t* modDirectory;
	void (*logInfo)(const char* message);
	void (*logWarn)(const char* message);
	void (*logError)(const char* message);
};

extern "C" {
	typedef const D2RLoaderPluginInfo* (__cdecl* D2RLoaderGetPluginInfo_t)() noexcept;
	typedef bool (__cdecl* D2RLoaderLoadHooks_t)(const D2RLoaderPluginContext* context) noexcept;
	typedef void (__cdecl* D2RLoaderUnload_t)() noexcept;
}

inline void D2RPluginLogV(const D2RLoaderPluginContext* context, void (*log)(const char*), const char* format, va_list args) noexcept {
	if (!context || !log || !format) {
		return;
	}

	char message[1024] {};
	std::vsnprintf(message, sizeof(message), format, args);
	log(message);
}

inline void D2RPluginLogInfoF(const D2RLoaderPluginContext* context, const char* format, ...) noexcept {
	va_list args;
	va_start(args, format);
	D2RPluginLogV(context, context ? context->logInfo : nullptr, format, args);
	va_end(args);
}

inline void D2RPluginLogWarnF(const D2RLoaderPluginContext* context, const char* format, ...) noexcept {
	va_list args;
	va_start(args, format);
	D2RPluginLogV(context, context ? context->logWarn : nullptr, format, args);
	va_end(args);
}

inline void D2RPluginLogErrorF(const D2RLoaderPluginContext* context, const char* format, ...) noexcept {
	va_list args;
	va_start(args, format);
	D2RPluginLogV(context, context ? context->logError : nullptr, format, args);
	va_end(args);
}
