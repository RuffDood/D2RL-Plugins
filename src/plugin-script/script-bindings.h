#pragma once

#include "script-private.h"

// Construct and register the "trap" global object on ctx.
// Must be called after Script_RegisterGameTypes.
void Script_RegisterTrapObject(JSContext* ctx);
