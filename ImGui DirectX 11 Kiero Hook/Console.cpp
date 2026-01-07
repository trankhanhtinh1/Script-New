#include "Console.h"
#include <Windows.h>
#include <iostream>

static FILE* g_consoleFile = nullptr;

namespace Console {
    void Init() {
        __try {
            AllocConsole();
            freopen_s(&g_consoleFile, "CONOUT$", "w", stdout);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void Close() {
        __try {
            if (g_consoleFile) {
                fclose(g_consoleFile);
                g_consoleFile = nullptr;
            }
            FreeConsole();
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void print(const std::string& msg) {
    __try {
        std::cout << msg << std::endl;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}
