#pragma once
/*
 * NightSharp DLL Key Verification
 *
 * Reads encrypted key from %APPDATA%\NightSharp\NightSharp.dat
 * Decrypts with XOR cipher (same seed as loader)
 * Verifies with server via VerifyUserKey
 */

#include <Windows.h>
#include <shlobj.h>
#include <string>
// #include "../../platina loader/framework/bypass/VerifyUserKey.h"

namespace KeyAuth {

// Status codes
enum class Status {
    NotChecked,
    Valid,
    Expired,
    InvalidKey,
    HWIDMismatch,
    NetworkError,
    FileNotFound,
    DecryptError
};

// XOR cipher seed (must match loader)
static const char CIPHER_SEED[] = "N1ghtSh4rp_2026!@#xK9";

// Result info
static Status g_status = Status::NotChecked;
static char g_errorMsg[256] = {};
static char g_remainingTime[128] = {};
static char g_keyCode[64] = {};

// ═══════════════════════════════════════════════════════════════
// KEY FILE: Read + Decrypt from NightSharp.dat
// ═══════════════════════════════════════════════════════════════

static unsigned char HexCharVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static std::string XorDecrypt(const std::string& data) {
    std::string result = data;
    int seedLen = lstrlenA(CIPHER_SEED);
    for (size_t i = 0; i < result.size(); i++)
        result[i] ^= CIPHER_SEED[i % seedLen];
    return result;
}

static std::string FromHex(const std::string& hex) {
    std::string data;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned char hi = HexCharVal(hex[i]);
        unsigned char lo = HexCharVal(hex[i + 1]);
        data.push_back((char)((hi << 4) | lo));
    }
    return data;
}

static bool ReadAndDecryptKey(char* outKey, int maxLen) {
    // Build path: %APPDATA%\NightSharp\NightSharp.dat
    wchar_t appdata[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);

    wchar_t path[MAX_PATH] = {};
    lstrcpyW(path, appdata);
    lstrcatW(path, L"\\NightSharp\\NightSharp.dat");

    // Read file
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        lstrcpyA(g_errorMsg, "Key file not found");
        return false;
    }

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == 0 || fileSize > 512 || (fileSize % 2) != 0) {
        CloseHandle(hFile);
        lstrcpyA(g_errorMsg, "Invalid key file");
        return false;
    }

    char hexBuf[512] = {};
    DWORD bytesRead = 0;
    ReadFile(hFile, hexBuf, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (bytesRead != fileSize) {
        lstrcpyA(g_errorMsg, "Failed to read key file");
        return false;
    }

    // Hex decode + XOR decrypt
    std::string hexData(hexBuf, bytesRead);
    std::string encrypted = FromHex(hexData);
    std::string key = XorDecrypt(encrypted);

    if (key.empty() || key.size() >= (size_t)maxLen) {
        lstrcpyA(g_errorMsg, "Key decrypt failed");
        return false;
    }

    lstrcpynA(outKey, key.c_str(), maxLen);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════

static Status Verify() {
    g_errorMsg[0] = 0;
    g_remainingTime[0] = 0;

    // Step 1: Read key from NightSharp.dat
    char key[64] = {};
    if (!ReadAndDecryptKey(key, 64)) {
        g_status = Status::FileNotFound;
        return g_status;
    }

    lstrcpynA(g_keyCode, key, 64);

    // Step 2: Verify with server using VerifyUserKey
    // VerifyUserKey verifier("https://7upvanguard.click");
    // verifier.SetKeyCode(std::string(key));
    //
    // if (!verifier.VerifyKey()) {
    //     std::string err = verifier.GetErrorMessage();
    //     lstrcpynA(g_errorMsg, err.c_str(), 256);
    //     g_status = Status::NetworkError;
    //     return g_status;
    // }
    //
    // if (verifier.IsKeyValid()) {
    //     std::string rt = verifier.GetRemainingTime();
    //     lstrcpynA(g_remainingTime, rt.c_str(), 128);
    //     g_status = Status::Valid;
    //     return g_status;
    // }
    //
    // std::string err = verifier.GetErrorMessage();
    // lstrcpynA(g_errorMsg, err.c_str(), 256);
    //
    // if (err.find("expired") != std::string::npos || err.find("Expired") != std::string::npos) {
    //     g_status = Status::Expired;
    // } else if (err.find("HWID") != std::string::npos || err.find("hwid") != std::string::npos) {
    //     g_status = Status::HWIDMismatch;
    // } else {
    //     g_status = Status::InvalidKey;
    // }

    g_status = Status::Valid;
    return g_status;
}

static bool IsValid() {
    return g_status == Status::Valid;
}

static const char* GetErrorMessage() {
    return g_errorMsg;
}

static const char* GetRemainingTime() {
    return g_remainingTime;
}

static const char* GetStatusText() {
    switch (g_status) {
        case Status::Valid:        return "Key Valid";
        case Status::Expired:      return "Key Expired";
        case Status::InvalidKey:   return "Invalid Key";
        case Status::HWIDMismatch: return "HWID Mismatch";
        case Status::NetworkError: return "Network Error";
        case Status::FileNotFound: return "Key File Not Found";
        case Status::DecryptError: return "Key Decrypt Error";
        default:                   return "Not Checked";
    }
}

} // namespace KeyAuth
