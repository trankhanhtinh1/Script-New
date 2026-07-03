#pragma once

#include <cstdint>

namespace SDK {

enum class KeyBindType : std::int32_t {
    Toggle = 0,
    Press = 1,
};

namespace Keys {
    inline constexpr int LButton = 0x01;
    inline constexpr int LMB = LButton;
    inline constexpr int RButton = 0x02;
    inline constexpr int RMB = RButton;
    inline constexpr int MButton = 0x04;
    inline constexpr int MMB = MButton;
    inline constexpr int XButton1 = 0x05;
    inline constexpr int XButton2 = 0x06;
    inline constexpr int Back = 0x08;
    inline constexpr int Tab = 0x09;
    inline constexpr int Enter = 0x0D;
    inline constexpr int Shift = 0x10;
    inline constexpr int Control = 0x11;
    inline constexpr int Ctrl = Control;
    inline constexpr int Alt = 0x12;
    inline constexpr int CapsLock = 0x14;
    inline constexpr int Escape = 0x1B;
    inline constexpr int Space = 0x20;
    inline constexpr int Insert = 0x2D;
    inline constexpr int Delete = 0x2E;

    inline constexpr int D0 = '0';
    inline constexpr int D1 = '1';
    inline constexpr int D2 = '2';
    inline constexpr int D3 = '3';
    inline constexpr int D4 = '4';
    inline constexpr int D5 = '5';
    inline constexpr int D6 = '6';
    inline constexpr int D7 = '7';
    inline constexpr int D8 = '8';
    inline constexpr int D9 = '9';

    inline constexpr int A = 'A';
    inline constexpr int B = 'B';
    inline constexpr int C = 'C';
    inline constexpr int D = 'D';
    inline constexpr int E = 'E';
    inline constexpr int F = 'F';
    inline constexpr int G = 'G';
    inline constexpr int H = 'H';
    inline constexpr int I = 'I';
    inline constexpr int J = 'J';
    inline constexpr int K = 'K';
    inline constexpr int L = 'L';
    inline constexpr int M = 'M';
    inline constexpr int N = 'N';
    inline constexpr int O = 'O';
    inline constexpr int P = 'P';
    inline constexpr int Q = 'Q';
    inline constexpr int R = 'R';
    inline constexpr int S = 'S';
    inline constexpr int T = 'T';
    inline constexpr int U = 'U';
    inline constexpr int V = 'V';
    inline constexpr int W = 'W';
    inline constexpr int X = 'X';
    inline constexpr int Y = 'Y';
    inline constexpr int Z = 'Z';

    inline constexpr int F1 = 0x70;
    inline constexpr int F2 = 0x71;
    inline constexpr int F3 = 0x72;
    inline constexpr int F4 = 0x73;
    inline constexpr int F5 = 0x74;
    inline constexpr int F6 = 0x75;
    inline constexpr int F7 = 0x76;
    inline constexpr int F8 = 0x77;
    inline constexpr int F9 = 0x78;
    inline constexpr int F10 = 0x79;
    inline constexpr int F11 = 0x7A;
    inline constexpr int F12 = 0x7B;
} // namespace Keys

} // namespace SDK
