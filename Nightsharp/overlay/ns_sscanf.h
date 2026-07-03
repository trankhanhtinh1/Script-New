#pragma once
/*
 * NightSharp v2.0 — Minimal sscanf replacement
 *
 * Replaces CRT sscanf with a self-contained implementation that
 * covers the subset used by ImGui's parser call sites:
 *   - imgui.cpp: "Pos=%i,%i", "Size=%i,%i", "Collapsed=%d", "%X"
 *   - imgui_widgets.cpp: "%02X%02X%02X%02X", "%02X%02X%02X", data-type fmt
 *   - imgui_tables.cpp (if included): "0x%08X,%d", "RefScale=%f", etc.
 *
 * Supported format specifiers: %d, %i, %u, %x, %X, %f, %c, %n, %%
 * Supports width modifiers (e.g., %08X, %02X)
 *
 * Uses <stdarg.h> which is a compiler builtin header, NOT CRT.
 */

#include <stdarg.h>   // va_list, va_start, va_arg, va_end — compiler builtin

#ifdef __cplusplus
extern "C" {
#endif

inline int ns_isspace_q(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
inline int ns_isdigit_q(int c) { return c >= '0' && c <= '9'; }
inline int ns_isxdigit_q(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline int ns_hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * Minimal sscanf implementation using proper va_list.
 * Returns number of successfully matched items (like CRT sscanf).
 */
inline int ns_sscanf(const char* buf, const char* fmt, ...) {
    const char* s = buf;
    const char* f = fmt;
    int matched = 0;

    va_list args;
    va_start(args, fmt);

    while (*f && *s) {
        // Literal match
        if (*f != '%') {
            if (ns_isspace_q(*f)) {
                while (ns_isspace_q(*s)) s++;
                f++;
                continue;
            }
            if (*f != *s) break;
            f++; s++;
            continue;
        }

        f++; // skip '%'
        if (*f == '%') { // literal %
            if (*s != '%') break;
            f++; s++;
            continue;
        }

        // Parse width (optional)
        int width = 0;
        if (*f == '0') f++; // skip leading 0 in width spec like %08X
        while (ns_isdigit_q(*f)) {
            width = width * 10 + (*f - '0');
            f++;
        }

        // Parse length modifier (optional, ignored for our purposes)
        if (*f == 'l') f++;
        if (*f == 'h') f++;

        // Parse conversion specifier
        switch (*f) {
        case 'd':
        case 'i': {
            while (ns_isspace_q(*s)) s++;
            int sign = 1;
            if (*s == '-') { sign = -1; s++; }
            else if (*s == '+') { s++; }
            if (!ns_isdigit_q(*s)) goto done;
            int val = 0, digits = 0;
            while (ns_isdigit_q(*s) && (!width || digits < width)) {
                val = val * 10 + (*s - '0');
                s++; digits++;
            }
            int* out = va_arg(args, int*);
            *out = sign * val;
            matched++;
            break;
        }
        case 'u': {
            while (ns_isspace_q(*s)) s++;
            if (!ns_isdigit_q(*s)) goto done;
            unsigned int val = 0, digits = 0;
            while (ns_isdigit_q(*s) && (!width || (int)digits < width)) {
                val = val * 10 + (*s - '0');
                s++; digits++;
            }
            unsigned int* out = va_arg(args, unsigned int*);
            *out = val;
            matched++;
            break;
        }
        case 'x':
        case 'X': {
            while (ns_isspace_q(*s)) s++;
            // Skip optional 0x prefix
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
            if (!ns_isxdigit_q(*s)) goto done;
            unsigned int val = 0, digits = 0;
            while (ns_isxdigit_q(*s) && (!width || (int)digits < width)) {
                val = val * 16 + ns_hex_val(*s);
                s++; digits++;
            }
            unsigned int* out = va_arg(args, unsigned int*);
            *out = val;
            matched++;
            break;
        }
        case 'f': {
            while (ns_isspace_q(*s)) s++;
            float sign = 1.0f;
            if (*s == '-') { sign = -1.0f; s++; }
            else if (*s == '+') { s++; }
            if (!ns_isdigit_q(*s) && *s != '.') goto done;
            float val = 0.0f, frac = 0.0f, div = 1.0f;
            while (ns_isdigit_q(*s)) {
                val = val * 10.0f + (*s - '0');
                s++;
            }
            if (*s == '.') {
                s++;
                while (ns_isdigit_q(*s)) {
                    frac = frac * 10.0f + (*s - '0');
                    div *= 10.0f;
                    s++;
                }
            }
            float* out = va_arg(args, float*);
            *out = sign * (val + frac / div);
            matched++;
            break;
        }
        case 'c': {
            char* out = va_arg(args, char*);
            *out = *s;
            s++;
            matched++;
            break;
        }
        case 'n': {
            int* out = va_arg(args, int*);
            *out = (int)(s - buf);
            // %n does NOT increment matched count (per C standard)
            break;
        }
        default:
            goto done;
        }
        f++;
    }

done:
    va_end(args);
    return matched;
}

#ifdef __cplusplus
}
#endif
