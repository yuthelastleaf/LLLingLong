#include "../include/lua_bindings.h"
#include <iostream>
#include <fstream>

namespace LuaBindings {

void registerPetAPI(sol::state& lua) {
    // Open standard Lua libraries
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, 
                       sol::lib::string, sol::lib::table);
    
    // Get API singleton
    auto& api = PetAPI::API::getInstance();
    
    // Create pet namespace in Lua
    lua["pet"] = lua.create_table();
    
    // System operations
    lua["pet"]["openProgram"] = [&api](const std::string& path) {
        return api.openProgram(path);
    };
    
    lua["pet"]["shutdown"] = [&api]() {
        api.shutdown();
    };
    
    // Pet control
    lua["pet"]["setPosition"] = [&api](int x, int y) {
        api.setPetPosition(x, y);
    };
    
    lua["pet"]["getPosition"] = [&lua]() -> sol::table {
        auto& api = PetAPI::API::getInstance();
        auto [x, y] = api.getPetPosition();
        sol::table result = lua.create_table();
        result["x"] = x;
        result["y"] = y;
        return result;
    };
    
    lua["pet"]["playAnimation"] = [&api](const std::string& animName) {
        api.playAnimation(animName);
    };
    
    lua["pet"]["getCurrentAnimation"] = [&api]() {
        return api.getCurrentAnimation();
    };
    
    // Communication
    lua["pet"]["showMessage"] = [&api](const std::string& message) {
        api.showMessage(message);
    };
    
    lua["pet"]["log"] = [&api](const std::string& message) {
        api.log(message);
    };
    
    // Utility
    lua["pet"]["getTime"] = [&api]() {
        return api.getTime();
    };
    
    // Helper function for easier position access
    lua.script(R"(
        function pet.moveTo(x, y)
            pet.setPosition(x, y)
            pet.log("Moved to position: " .. x .. ", " .. y)
        end
        
        function pet.say(message)
            pet.log("[Pet says]: " .. message)
            pet.showMessage(message)
        end
    )");
    
    std::cout << "Lua API registered successfully" << std::endl;
}

bool loadScript(sol::state& lua, const std::string& scriptPath) {
    try {
        lua.script_file(scriptPath);
        std::cout << "Loaded script: " << scriptPath << std::endl;
        return true;
    } catch (const sol::error& e) {
        std::cerr << "Lua error loading " << scriptPath << ": " << e.what() << std::endl;
        return false;
    }
}

} // namespace LuaBindings
