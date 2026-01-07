#include "Console.h"
#include <Windows.h>
#include <iostream>

static FILE* g_consoleFile = nullptr;

namespace Console {
    void Init() {
        AllocConsole();
        freopen_s(&g_consoleFile, "CONOUT$", "w", stdout);
    }

    void Close() {
        if (g_consoleFile) {
            fclose(g_consoleFile);
            g_consoleFile = nullptr;
        }
        FreeConsole();
    }
}

void print(const std::string& msg) {
    std::cout << msg << std::endl;
}
