#pragma once

#include "script-private.h"

// Call once after JS_NewContext to register D2Unit and D2Game class definitions.
void Script_RegisterGameTypes(JSContext* ctx);

// Wrap a native D2UnitStrc pointer in a JS object.
// The returned JSValue has refcount 1; the caller owns the reference.
// The native pointer must remain valid for the lifetime of the callback invocation.
JSValue Script_MakeUnitObject(JSContext* ctx, D2UnitStrc* unit);

// Wrap a native D2GameStrc pointer in a JS object.
// Same ownership rules as Script_MakeUnitObject.
JSValue Script_MakeGameObject(JSContext* ctx, D2GameStrc* game);
