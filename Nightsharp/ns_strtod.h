#pragma once
/*
 * NightSharp v2.0 — Minimal strtod replacement
 *
 * Replaces CRT strtod with a self-contained implementation.
 * Only needs to parse simple decimal floats (with optional exponent).
 * No CRT dependency — uses only basic C operators.
 */

#ifdef __cplusplus
extern "C" {
#endif

inline double ns_strtod(const char* str, char** endptr) {
    const char* s = str;
    // Skip whitespace
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;

    double sign = 1.0;
    if (*s == '-') { sign = -1.0; s++; }
    else if (*s == '+') { s++; }

    double val = 0.0;
    int has_digits = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0 + (*s - '0');
        s++;
        has_digits = 1;
    }
    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (*s >= '0' && *s <= '9') {
            val += (*s - '0') * frac;
            frac *= 0.1;
            s++;
            has_digits = 1;
        }
    }
    // Handle simple exponent (e.g., 1e5, 2.5e-3)
    if (has_digits && (*s == 'e' || *s == 'E')) {
        s++;
        int exp_sign = 1;
        if (*s == '-') { exp_sign = -1; s++; }
        else if (*s == '+') { s++; }
        int exp_val = 0;
        while (*s >= '0' && *s <= '9') {
            exp_val = exp_val * 10 + (*s - '0');
            s++;
        }
        double mul = 1.0;
        for (int i = 0; i < exp_val; i++) mul *= 10.0;
        if (exp_sign < 0) val /= mul; else val *= mul;
    }

    if (endptr) *endptr = (char*)(has_digits ? s : str);
    return sign * val;
}

#ifdef __cplusplus
}
#endif
