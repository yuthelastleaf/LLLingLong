#pragma once

#include <sol/sol.hpp>
#include "pet_api.h"

// Lua bindings for Pet API
namespace LuaBindings {

// Initialize Lua state and bind all APIs
void registerPetAPI(sol::state& lua);

// Load and execute a Lua script file
bool loadScript(sol::state& lua, const std::string& scriptPath);

} // namespace LuaBindings
