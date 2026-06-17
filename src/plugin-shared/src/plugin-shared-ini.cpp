#include <Windows.h>
#include <plugin.h>
#include <plugin-shared.h>

static const wchar_t* PSh_Ini_GetBaseFileName()
{
	return L"D2RLoader.ini";
}

const wchar_t* PSh_Ini_GetString(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, const wchar_t* defaultValue)
{
	static wchar_t buffer[256];
	static const wchar_t sentinel[] = L"\x0001\x0002\x0003";

	GetPrivateProfileStringW(sectionName, optionName, sentinel, buffer, sizeof(buffer) / sizeof(wchar_t), context->modDirectory);
	if (wcscmp(buffer, sentinel) != 0)
		return buffer;

	GetPrivateProfileStringW(sectionName, optionName, sentinel, buffer, sizeof(buffer) / sizeof(wchar_t), PSh_Ini_GetBaseFileName());
	if (wcscmp(buffer, sentinel) != 0)
		return buffer;

	wcsncpy_s(buffer, sizeof(buffer) / sizeof(wchar_t), defaultValue ? defaultValue : L"", _TRUNCATE);
	return buffer;
}

int PSh_Ini_GetInt(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, int defaultValue)
{
	static const wchar_t sentinel[] = L"\x0001\x0002\x0003";
	wchar_t checkBuf[32];

	GetPrivateProfileStringW(sectionName, optionName, sentinel, checkBuf, sizeof(checkBuf) / sizeof(wchar_t), context->modDirectory);
	if (wcscmp(checkBuf, sentinel) != 0)
		return GetPrivateProfileIntW(sectionName, optionName, defaultValue, context->modDirectory);

	GetPrivateProfileStringW(sectionName, optionName, sentinel, checkBuf, sizeof(checkBuf) / sizeof(wchar_t), PSh_Ini_GetBaseFileName());
	if (wcscmp(checkBuf, sentinel) != 0)
		return GetPrivateProfileIntW(sectionName, optionName, defaultValue, PSh_Ini_GetBaseFileName());

	return defaultValue;
}

uint32_t PSh_Ini_GetItemCode(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, const wchar_t* defaultValue)
{
	const wchar_t* wcode = PSh_Ini_GetString(context, sectionName, optionName, defaultValue);
	char narrow[8] = {};
	WideCharToMultiByte(CP_ACP, 0, wcode, -1, narrow, sizeof(narrow), nullptr, nullptr);
	return PSh_EncodeItemCode(narrow);
}

uint32_t PSh_Ini_GetItemTypeCode(const D2RLoaderPluginContext* context, const wchar_t* sectionName, const wchar_t* optionName, const wchar_t* defaultValue)
{
	const wchar_t* wcode = PSh_Ini_GetString(context, sectionName, optionName, defaultValue);
	char narrow[8] = {};
	WideCharToMultiByte(CP_ACP, 0, wcode, -1, narrow, sizeof(narrow), nullptr, nullptr);
	return PSh_EncodeItemTypeCode(narrow);
}
