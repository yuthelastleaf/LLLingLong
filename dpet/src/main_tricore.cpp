// Desktop Pet - Tri-Core Architecture
// Main entry point

#include "../include/App.h"
#include <iostream>

int main(int argc, char* argv[]) {
    DesktopPet::App app;
    
    if (!app.Init()) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    app.Run();
    app.Shutdown();
    
    return 0;
}
