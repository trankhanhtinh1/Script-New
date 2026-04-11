#pragma once

#define CURL_STATICLIB

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>
#include "version.h"

using namespace AppNightSharp;
#pragma comment(lib, "iphlpapi.lib")
// Thử các option sau tùy theo phiên bản OpenSSL:
// Option 1: OpenSSL 3.x
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
// Option 2: OpenSSL 1.1.x (uncomment nếu option 1 không work)
//#pragma comment(lib, "libssl-1_1.lib")
//#pragma comment(lib, "libcrypto-1_1.lib")
// Option 3: OpenSSL 1.0.x (uncomment nếu option 1,2 không work)
//#pragma comment(lib, "ssleay32.lib")
//#pragma comment(lib, "libeay32.lib")


using json = nlohmann::json;

class VerifyUserKey {
private:
    const std::string API_ENDPOINT = "/api/login";
    std::string m_baseUrl = "https://7upvanguard.click";
    const std::string AES_KEY = "78881c6fcb9c88ad730713a46e04b43a";
    std::string m_keyCode;
    std::string m_hwid;
    std::string m_appVersion;
    
    // Kết quả xác thực
    bool m_isAuthenticated = false;
    std::string m_errorMessage;
    
    // Thông tin key
    int m_keyId = 0;
    std::string m_appName;
    std::string m_typeKey;
    std::string m_status;
    std::string m_timeActive;
    std::string m_expireTime;
    std::string m_lastUsed;
    bool m_isLifetime = false;
    std::string m_serverTime;
    std::string m_username;
    long m_remainingSeconds = 0;
    std::string m_remainingTime;
    
    // Thêm biến cho server version
    std::string m_serverVersion;
    
    // Callback cho CURL
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        try {
            s->append((char*)contents, newLength);
            return newLength;
        }
        catch (std::bad_alloc& e) {
            return 0;
        }
    }
    
    // Hàm giải mã Base64
    std::string DecodeBase64(const std::string& encoded) {
        BIO* bio = BIO_new_mem_buf(encoded.data(), -1);
        BIO* b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        
        std::vector<char> decoded(encoded.length());
        int decodedLength = BIO_read(bio, decoded.data(), decoded.size());
        BIO_free_all(bio);
        
        if (decodedLength > 0) {
            return std::string(decoded.begin(), decoded.begin() + decodedLength);
        }
        return "";
    }
    
    // Hash SHA256 của string (sử dụng OpenSSL)
    std::string HashSHA256(const std::string& str) {
        unsigned char hash[32];
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";
        
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        if (EVP_DigestUpdate(ctx, str.c_str(), str.length()) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        unsigned int hashLen;
        if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        
        EVP_MD_CTX_free(ctx);
        return std::string(reinterpret_cast<char*>(hash), 32);
    }
    
    // Hàm giải mã AES theo logic PHP
    std::string DecryptAES(const std::string& encryptedData, const std::string& key) {
        try {
            std::string decoded = DecodeBase64(encryptedData);
            if (decoded.empty()) {
                return "";
            }
            
            // Thử phương pháp PHP trước (SHA256 hash của key)
            std::string result = TryPHPMethod(decoded, key);
            if (!result.empty()) {
                return result;
            }
            
            // Fallback: thử các phương pháp khác
            std::vector<std::string> keys = {
                key,                          // "demokey123"
                key + key,                   // Key lặp lại
                std::string(32, key[0])      // Key đầu tiên lặp 32 lần
            };
            
            for (size_t keyIdx = 0; keyIdx < keys.size(); keyIdx++) {
                // Method 1: IV từ 16 bytes đầu + AES-256-CBC
                if (decoded.length() > 16) {
                    std::string result = TryDecryptWithKeyVariant(decoded, keys[keyIdx], 1);
                    if (!result.empty()) {
                        return result;
                    }
                }
                
                // Method 2: IV zero + AES-256-CBC
                std::string result = TryDecryptWithKeyVariant(decoded, keys[keyIdx], 2);
                if (!result.empty()) {
                    return result;
                }
                
                // Method 3: AES-128-CBC
                result = TryDecryptWithKeyVariant(decoded, keys[keyIdx], 3);
                if (!result.empty()) {
                    return result;
                }
                
                // Method 4: AES-256-ECB (không dùng IV)
                result = TryDecryptWithKeyVariant(decoded, keys[keyIdx], 4);
                if (!result.empty()) {
                    return result;
                }
                
                // Method 5: AES-128-ECB
                result = TryDecryptWithKeyVariant(decoded, keys[keyIdx], 5);
                if (!result.empty()) {
                    return result;
                }
            }
            return "";
            
        } catch (const std::exception& e) {
            return "";
        }
    }
    
    // Phương pháp giải mã theo logic PHP
    std::string TryPHPMethod(const std::string& decoded, const std::string& key) {
        try {
            if (decoded.length() <= 16) {
                return "";
            }
            
            // Tạo SHA256 hash của key (32 bytes)
            std::string hashedKey = HashSHA256(key);
            if (hashedKey.empty()) {
                return "";
            }
            
            // IV = 16 bytes đầu
            std::vector<unsigned char> iv(decoded.begin(), decoded.begin() + 16);
            
            // Encrypted data = phần còn lại
            std::vector<unsigned char> ciphertext(decoded.begin() + 16, decoded.end());
            
            // Key = SHA256 hash (32 bytes)
            std::vector<unsigned char> aesKey(hashedKey.begin(), hashedKey.end());
            
            // Giải mã bằng AES-256-CBC
            return PerformAESDecrypt(ciphertext, aesKey, iv, EVP_aes_256_cbc());
            
        } catch (const std::exception& e) {
            return "";
        }
    }
    
    // Thử giải mã với variant key khác nhau
    std::string TryDecryptWithKeyVariant(const std::string& decoded, const std::string& key, int method) {
        try {
            switch (method) {
                case 1: return TryDecryptMethod1(decoded, key);
                case 2: return TryDecryptMethod2(decoded, key);
                case 3: return TryDecryptMethod3(decoded, key);
                case 4: return TryDecryptMethod4(decoded, key); // ECB-256
                case 5: return TryDecryptMethod5(decoded, key); // ECB-128
                default: return "";
            }
        } catch (...) {
            return "";
        }
    }
    
    // Phương pháp 1: IV 16 bytes đầu
    std::string TryDecryptMethod1(const std::string& decoded, const std::string& key) {
        try {
            std::vector<unsigned char> iv(decoded.begin(), decoded.begin() + 16);
            std::vector<unsigned char> ciphertext(decoded.begin() + 16, decoded.end());
            
            // Chuẩn bị key 32 bytes
            std::vector<unsigned char> aesKey(32, 0);
            for (size_t i = 0; i < key.length() && i < 32; i++) {
                aesKey[i] = key[i];
            }
            
            return PerformAESDecrypt(ciphertext, aesKey, iv, EVP_aes_256_cbc());
        } catch (...) {
            return "";
        }
    }
    
    // Phương pháp 2: IV zero
    std::string TryDecryptMethod2(const std::string& decoded, const std::string& key) {
        try {
            std::vector<unsigned char> ciphertext(decoded.begin(), decoded.end());
            std::vector<unsigned char> iv(16, 0); // IV zero
            
            // Chuẩn bị key 32 bytes
            std::vector<unsigned char> aesKey(32, 0);
            for (size_t i = 0; i < key.length() && i < 32; i++) {
                aesKey[i] = key[i];
            }
            
            return PerformAESDecrypt(ciphertext, aesKey, iv, EVP_aes_256_cbc());
        } catch (...) {
            return "";
        }
    }
    
    // Phương pháp 3: AES-128
    std::string TryDecryptMethod3(const std::string& decoded, const std::string& key) {
        try {
            std::vector<unsigned char> ciphertext(decoded.begin(), decoded.end());
            std::vector<unsigned char> iv(16, 0); // IV zero
            
            // Chuẩn bị key 16 bytes cho AES-128
            std::vector<unsigned char> aesKey(16, 0);
            for (size_t i = 0; i < key.length() && i < 16; i++) {
                aesKey[i] = key[i];
            }
            
            return PerformAESDecrypt(ciphertext, aesKey, iv, EVP_aes_128_cbc());
        } catch (...) {
            return "";
        }
    }
    
    // Phương pháp 4: AES-256-ECB (không cần IV)
    std::string TryDecryptMethod4(const std::string& decoded, const std::string& key) {
        try {
            std::vector<unsigned char> ciphertext(decoded.begin(), decoded.end());
            
            // Chuẩn bị key 32 bytes
            std::vector<unsigned char> aesKey(32, 0);
            for (size_t i = 0; i < key.length() && i < 32; i++) {
                aesKey[i] = key[i];
            }
            
            // Thử với padding
            std::string result = PerformAESDecryptECB(ciphertext, aesKey, EVP_aes_256_ecb(), true);
            if (!result.empty()) return result;
            
            // Thử không padding
            return PerformAESDecryptECB(ciphertext, aesKey, EVP_aes_256_ecb(), false);
        } catch (...) {
            return "";
        }
    }
    
    // Phương pháp 5: AES-128-ECB
    std::string TryDecryptMethod5(const std::string& decoded, const std::string& key) {
        try {
            std::vector<unsigned char> ciphertext(decoded.begin(), decoded.end());
            
            // Chuẩn bị key 16 bytes
            std::vector<unsigned char> aesKey(16, 0);
            for (size_t i = 0; i < key.length() && i < 16; i++) {
                aesKey[i] = key[i];
            }
            
            // Thử với padding
            std::string result = PerformAESDecryptECB(ciphertext, aesKey, EVP_aes_128_ecb(), true);
            if (!result.empty()) return result;
            
            // Thử không padding
            return PerformAESDecryptECB(ciphertext, aesKey, EVP_aes_128_ecb(), false);
        } catch (...) {
            return "";
        }
    }
    
    // Hàm thực hiện giải mã AES CBC mode
    std::string PerformAESDecrypt(const std::vector<unsigned char>& ciphertext, 
                                  const std::vector<unsigned char>& key,
                                  const std::vector<unsigned char>& iv,
                                  const EVP_CIPHER* cipher) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";
        
        if (EVP_DecryptInit_ex(ctx, cipher, NULL, key.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        std::vector<unsigned char> plaintext(ciphertext.size() + EVP_CIPHER_block_size(cipher));
        int len;
        int plaintextLen;
        
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintextLen = len;
        
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintextLen += len;
        
        EVP_CIPHER_CTX_free(ctx);
        
        std::string result(plaintext.begin(), plaintext.begin() + plaintextLen);
        
        // Kiểm tra xem có phải JSON hợp lệ không
        if (result.find('{') != std::string::npos || result.find("success") != std::string::npos) {
            return result;
        }
        
        return "";
    }
    
    // Hàm thực hiện giải mã AES ECB mode (không cần IV)
    std::string PerformAESDecryptECB(const std::vector<unsigned char>& ciphertext, 
                                     const std::vector<unsigned char>& key,
                                     const EVP_CIPHER* cipher,
                                     bool usePadding = true) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";
        
        if (EVP_DecryptInit_ex(ctx, cipher, NULL, key.data(), NULL) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        // Set padding mode
        EVP_CIPHER_CTX_set_padding(ctx, usePadding ? 1 : 0);
        
        std::vector<unsigned char> plaintext(ciphertext.size() + EVP_CIPHER_block_size(cipher));
        int len;
        int plaintextLen;
        
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintextLen = len;
        
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintextLen += len;
        
        EVP_CIPHER_CTX_free(ctx);
        
        // Nếu không dùng padding, loại bỏ null bytes cuối
        if (!usePadding) {
            while (plaintextLen > 0 && plaintext[plaintextLen - 1] == 0) {
                plaintextLen--;
            }
        }
        
        std::string result(plaintext.begin(), plaintext.begin() + plaintextLen);
        
        // Kiểm tra xem có phải JSON hợp lệ không
        if (result.find('{') != std::string::npos && result.find('}') != std::string::npos) {
            return result;
        }
        
        return "";
    }
    
    // Hàm tạo hash đơn giản
    unsigned int SimpleHash(const std::string& str) {
        unsigned int hash = 5381;
        for (char c : str) {
            hash = ((hash << 5) + hash) + c; // hash * 33 + c
        }
        return hash;
    }
    
    // Lấy Hardware ID của thiết bị
    std::string GetHardwareID() {
        std::stringstream hwid;
        
        // Lấy thông tin CPU
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        // Lấy thông tin volume
        char volumeName[MAX_PATH + 1] = { 0 };
        char fileSystemName[MAX_PATH + 1] = { 0 };
        DWORD serialNumber = 0;
        GetVolumeInformationA("C:\\", volumeName, ARRAYSIZE(volumeName),
                             &serialNumber, NULL, NULL, fileSystemName, ARRAYSIZE(fileSystemName));
        
        // Lấy thông tin computer name
        char computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD size = sizeof(computerName) / sizeof(computerName[0]);
        GetComputerNameA(computerName, &size);
        
        // Lấy thông tin MAC address
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD dwBufLen = sizeof(adapterInfo);
        DWORD dwStatus = GetAdaptersInfo(adapterInfo, &dwBufLen);
        std::string macAddress;
        
        if (dwStatus == ERROR_SUCCESS) {
            PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
            if (pAdapterInfo) {
                std::stringstream macStream;
                for (int i = 0; i < pAdapterInfo->AddressLength; i++) {
                    if (i > 0) macStream << ":";
                    macStream << std::hex << std::setw(2) << std::setfill('0') 
                              << (int)pAdapterInfo->Address[i];
                }
                macAddress = macStream.str();
            }
        }
        
        // Kết hợp các thông tin
        std::string baseInfo = std::string("CPU:") + std::to_string(sysInfo.dwProcessorType) + "-" + 
                             std::to_string(sysInfo.dwNumberOfProcessors) + "-VOL:" + 
                             std::to_string(serialNumber) + "-PC:" + computerName + "-MAC:" + macAddress;
        
        // Tạo hash đơn giản từ thông tin phần cứng
        unsigned int hash = SimpleHash(baseInfo);
        
        // Tạo HWID dạng hex
        hwid << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << hash;
        
        // Thêm thông tin bổ sung để làm cho HWID dài hơn và phức tạp hơn
        hwid << "-" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (serialNumber & 0xFFFF);
        hwid << "-" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (sysInfo.dwProcessorType & 0xFFFF);
        hwid << "-" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << SimpleHash(macAddress);
        
        return hwid.str();
    }
    
public:
    VerifyUserKey(const std::string& baseUrl, const std::string& appVersion = APP_VERSION) 
        : m_baseUrl(baseUrl), m_appVersion(appVersion), m_appName("NightSharp") {
        m_hwid = GetHardwareID();
    }
    
    // Thiết lập key cần xác thực
    void SetKeyCode(const std::string& keyCode) {
        m_keyCode = keyCode;
    }
    
    // Thiết lập Hardware ID thủ công (nếu cần)
    void SetHWID(const std::string& hwid) {
        m_hwid = hwid;
    }
    
    // Xác thực key với API
    bool VerifyKey() {
        if (m_keyCode.empty()) {
            m_errorMessage = "Key code is required";
            return false;
        }
        
        if (m_hwid.empty()) {
            m_errorMessage = "Hardware ID is required";
            return false;
        }
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            m_errorMessage = "Failed to initialize CURL";
            return false;
        }
        
        // Tạo JSON request body
        json requestData;
        requestData["key_code"] = m_keyCode;
        requestData["hwid"] = m_hwid;
        requestData["app_version"] = m_appVersion;
        std::string jsonBody = requestData.dump();
        
        // Thiết lập CURL request
        std::string responseBuffer;
        std::string url = m_baseUrl + API_ENDPOINT;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
        
        // Thiết lập headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("User-Agent: NightSharp/" + m_appVersion).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Thực hiện request
        CURLcode res = curl_easy_perform(curl);
        
        // Kiểm tra kết quả
        if (res != CURLE_OK) {
            m_errorMessage = curl_easy_strerror(res);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return false;
        }
        
        // Lấy HTTP status code
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        
        // Giải phóng tài nguyên CURL
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        

        
        // Xử lý response
        try {
            // Đầu tiên thử parse JSON
            json response;
            bool isJsonResponse = true;
            
            try {
                response = json::parse(responseBuffer);
            } catch (const std::exception&) {
                // Không phải JSON, có thể là dữ liệu mã hóa trực tiếp
                isJsonResponse = false;
            }
            
            std::string decryptedData;
            
            if (isJsonResponse && response.contains("encrypted_data")) {
                // Trường hợp 1: JSON với field encrypted_data
                std::string encryptedData = response["encrypted_data"].get<std::string>();
                decryptedData = DecryptAES(encryptedData, AES_KEY);
            } else if (!isJsonResponse) {
                // Trường hợp 2: Toàn bộ response là dữ liệu mã hóa AES
                decryptedData = DecryptAES(responseBuffer, AES_KEY);
            }
            
            if (!decryptedData.empty()) {
                // Có dữ liệu đã giải mã, parse JSON
                try {
                    json decryptedResponse = json::parse(decryptedData);
                    
                    if (decryptedResponse.contains("success") && decryptedResponse["success"].get<bool>()) {
                        // Xác thực thành công
                        m_isAuthenticated = true;
                        
                        // Lưu thông tin key với kiểm tra null cho tất cả string fields
                        json data = decryptedResponse["data"];
                        m_keyId = data.contains("key_id") && !data["key_id"].is_null() ? data["key_id"].get<int>() : 0;
                        m_keyCode = data.contains("key_code") && !data["key_code"].is_null() ? 
                                   data["key_code"].get<std::string>() : "";
                        m_appName = data.contains("app_name") && !data["app_name"].is_null() ? 
                                   data["app_name"].get<std::string>() : "";
                        m_typeKey = data.contains("type_key") && !data["type_key"].is_null() ? 
                                   data["type_key"].get<std::string>() : "";
                        m_status = data.contains("status") && !data["status"].is_null() ? 
                                  data["status"].get<std::string>() : "";
                        m_hwid = data.contains("hwid") && !data["hwid"].is_null() ? 
                                data["hwid"].get<std::string>() : "";
                        m_timeActive = data.contains("time_active") && !data["time_active"].is_null() ? 
                                      data["time_active"].get<std::string>() : "";
                        m_expireTime = data.contains("expire_time") && !data["expire_time"].is_null() ? 
                                       data["expire_time"].get<std::string>() : "";
                        m_lastUsed = data.contains("last_used") && !data["last_used"].is_null() ? 
                                    data["last_used"].get<std::string>() : "";
                        m_isLifetime = data.contains("is_lifetime") && !data["is_lifetime"].is_null() ? 
                                      data["is_lifetime"].get<bool>() : false;
                        m_serverTime = data.contains("server_time") && !data["server_time"].is_null() ? 
                                      data["server_time"].get<std::string>() : "";
                        m_username = data.contains("username") && !data["username"].is_null() ? 
                                     data["username"].get<std::string>() : "";
                        m_remainingSeconds = data.contains("remaining_seconds") && !data["remaining_seconds"].is_null() ? 
                                           data["remaining_seconds"].get<long>() : 0;
                        m_remainingTime = data.contains("remaining_time") && !data["remaining_time"].is_null() ? 
                                         data["remaining_time"].get<std::string>() : "";
                        
                        // Lưu server version nếu có
                        if (data.contains("server_version")) {
                            m_serverVersion = data["server_version"].get<std::string>();
                        }
                        
                        // Kiểm tra xem app_name có phải là 7UP2PC không
                        if (m_appName.empty() || m_appName != "NightSharp") {
                            m_isAuthenticated = false;
                            m_errorMessage = "Key Invalid";
                            return false;
                        }
                        
                        return true;
                    } else {
                        // Xác thực thất bại
                        m_isAuthenticated = false;
                        m_errorMessage = decryptedResponse.contains("message") ? 
                                         decryptedResponse["message"].get<std::string>() : "Unknown error";
                        return false;
                    }
                } catch (const std::exception& e) {
                    m_errorMessage = std::string("Failed to parse decrypted response: ") + e.what();
                    return false;
                }
            } else if (isJsonResponse) {
                // Xử lý JSON response không mã hóa (fallback)
                if (response.contains("success") && response["success"].get<bool>()) {
                    // Xác thực thành công
                    m_isAuthenticated = true;
                    
                    // Lưu thông tin key với kiểm tra null cho tất cả string fields
                    json data = response["data"];
                    m_keyId = data.contains("key_id") && !data["key_id"].is_null() ? data["key_id"].get<int>() : 0;
                    m_keyCode = data.contains("key_code") && !data["key_code"].is_null() ? 
                               data["key_code"].get<std::string>() : "";
                    m_appName = data.contains("app_name") && !data["app_name"].is_null() ? 
                               data["app_name"].get<std::string>() : "";
                    m_typeKey = data.contains("type_key") && !data["type_key"].is_null() ? 
                               data["type_key"].get<std::string>() : "";
                    m_status = data.contains("status") && !data["status"].is_null() ? 
                              data["status"].get<std::string>() : "";
                    m_hwid = data.contains("hwid") && !data["hwid"].is_null() ? 
                            data["hwid"].get<std::string>() : "";
                    m_timeActive = data.contains("time_active") && !data["time_active"].is_null() ? 
                                  data["time_active"].get<std::string>() : "";
                    m_expireTime = data.contains("expire_time") && !data["expire_time"].is_null() ? 
                                   data["expire_time"].get<std::string>() : "";
                    m_lastUsed = data.contains("last_used") && !data["last_used"].is_null() ? 
                                data["last_used"].get<std::string>() : "";
                    m_isLifetime = data.contains("is_lifetime") && !data["is_lifetime"].is_null() ? 
                                  data["is_lifetime"].get<bool>() : false;
                    m_serverTime = data.contains("server_time") && !data["server_time"].is_null() ? 
                                  data["server_time"].get<std::string>() : "";
                    m_username = data.contains("username") && !data["username"].is_null() ? 
                                 data["username"].get<std::string>() : "";
                    m_remainingSeconds = data.contains("remaining_seconds") && !data["remaining_seconds"].is_null() ? 
                                       data["remaining_seconds"].get<long>() : 0;
                    m_remainingTime = data.contains("remaining_time") && !data["remaining_time"].is_null() ? 
                                     data["remaining_time"].get<std::string>() : "";
                    
                    // Lưu server version nếu có
                    if (data.contains("server_version")) {
                        m_serverVersion = data["server_version"].get<std::string>();
                    }
                    
                    // Kiểm tra xem app_name có phải là 7UP2PC không
                    if (m_appName.empty() || m_appName != "NightSharp") {
                        m_isAuthenticated = false;
                        m_errorMessage = "Key Invalid";
                        return false;
                    }
                    
                    return true;
                } else {
                    // Xác thực thất bại
                    m_isAuthenticated = false;
                    m_errorMessage = response.contains("message") ? 
                                     response["message"].get<std::string>() : "Unknown error";
                    return false;
                }
            } else {
                // Không thể parse JSON và không giải mã được
                m_errorMessage = "Failed to decrypt response data with key: " + AES_KEY;
                return false;
            }
        } catch (const std::exception& e) {
            m_errorMessage = std::string("Failed to process response: ") + e.what();
            return false;
        }
    }
    
    // Kiểm tra xem key có còn hạn không
    bool IsKeyValid() const {
        return m_isAuthenticated && (m_status == "active");
    }
    
    // Kiểm tra xem key có phải lifetime không
    bool IsLifetime() const {
        return m_isAuthenticated && m_isLifetime;
    }
    
    // Lấy thời gian còn lại (giây)
    long GetRemainingSeconds() const {
        return m_remainingSeconds;
    }
    
    // Lấy thời gian còn lại (định dạng text)
    std::string GetRemainingTime() const {
        return m_remainingTime;
    }
    
    // Lấy thông báo lỗi nếu có
    std::string GetErrorMessage() const {
        return m_errorMessage;
    }
    
    // Lấy thông tin key
    std::string GetKeyType() const {
        return m_typeKey;
    }
    
    std::string GetUsername() const {
        return m_username;
    }
    
    std::string GetAppName() const {
        return m_appName;
    }
    
    std::string GetExpireTime() const {
        return m_expireTime;
    }
    
    // Kiểm tra xem key có bị ban không
    bool IsBanned() const {
        return m_status == "banned";
    }
    
    // Kiểm tra có cần update không
    bool NeedUpdate() const {
        return !m_serverVersion.empty() && m_serverVersion != m_appVersion;
    }
    
    // Lấy server version
    std::string GetServerVersion() const {
        return m_serverVersion;
    }
    
    // Download file callback
    static size_t WriteFileCallback(void* contents, size_t size, size_t nmemb, FILE* stream) {
        return fwrite(contents, size, nmemb, stream);
    }
    
    // Download file đơn giản - DEPRECATED: No longer used for automatic updates
    // Manual download is now required due to server access restrictions
    bool DownloadUpdate(const std::string& savePath = "") {
        // DEPRECATED: Automatic download is no longer supported
        // Please download manually from official channels
        m_errorMessage = "Automatic download is no longer supported. Please download manually from official channels.";
        return false;
    }
    
    // Reset HWID trên server
    bool ResetHWID() {
        if (m_keyCode.empty()) {
            m_errorMessage = "Key code is required";
            return false;
        }
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            m_errorMessage = "Failed to initialize CURL";
            return false;
        }
        
        // Build POST body
        std::string postData = "key_code=" + m_keyCode + "&hwid=" + m_hwid + "&app_name=NightSharp";
        
        std::string responseBuffer;
        std::string url = m_baseUrl + "/api/public_reset_hwid.php";
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            m_errorMessage = curl_easy_strerror(res);
            return false;
        }
        
        // Parse response
        try {
            json response = json::parse(responseBuffer);
            if (response.contains("success") && response["success"].get<bool>()) {
                return true;
            } else {
                m_errorMessage = response.contains("message") ? 
                    response["message"].get<std::string>() : "HWID reset failed";
                return false;
            }
        } catch (const std::exception& e) {
            m_errorMessage = std::string("Failed to parse response: ") + e.what();
            return false;
        }
    }
    
    // Lấy HWID hiện tại
    std::string GetCurrentHWID() const {
        return m_hwid;
    }
};
