#pragma once

#include <regex>

#include "rule.hpp"

// TODO: integrate into ...
namespace legacy {
    namespace _impl_details {
        inline char to_base64(uint8_t b6) {
            if (b6 < 26) {
                return 'A' + b6;
            } else if (b6 < 52) {
                return 'a' + b6 - 26;
            } else if (b6 < 62) {
                return '0' + b6 - 52;
            } else if (b6 == 62) {
                return '+';
            } else {
                assert(b6 == 63);
                return '/';
            }
        }

        inline uint8_t from_base64(char ch) {
            if (ch >= 'A' && ch <= 'Z') {
                return ch - 'A';
            } else if (ch >= 'a' && ch <= 'z') {
                return 26 + ch - 'a';
            } else if (ch >= '0' && ch <= '9') {
                return 52 + ch - '0';
            } else if (ch == '+') {
                return 62;
            } else {
                if (ch != '/') {
                    putchar(ch);
                }
                assert(ch == '/');
                return 63;
            }
        }
    } // namespace _impl_details

    // TODO: rephrase...
    // Convert ruleT into text form. Specifically, in "MAP" format, so that the result can be pasted into Golly.
    // The correctness can be verified by checking the result of `to_MAP_str(game_of_life())`.
    //
    // Source: https://golly.sourceforge.io/Help/Algorithms/QuickLife.html
    // > So, Conway's Life (B3/S23) encoded as a MAP rule is:
    // > rule = MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
    inline string to_MAP_str(const ruleT& rule) {
        // MAP rule uses a different coding scheme.
        bool reordered[512]{};
        for (int code = 0; code < 512; ++code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            reordered[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1] = rule[code];
        }
        auto get = [&reordered](int i) { return i < 512 ? reordered[i] : 0; };
        string str = "MAP";
        int p = 0;
        while (p < 512) {
            uint8_t b6 = (get(p + 5) << 0) | (get(p + 4) << 1) | (get(p + 3) << 2) | (get(p + 2) << 3) |
                         (get(p + 1) << 4) | (get(p + 0) << 5);
            str += _impl_details::to_base64(b6);
            p += 6;
        }
        return str;
    }

    inline const std::regex& regex_MAP_str() {
        static_assert((512 + 5) / 6 == 86);
        static std::regex reg{"MAP[a-zA-Z0-9+/]{86}", std::regex_constants::optimize};
        return reg;
    }

    inline ruleT from_MAP_str(const string& map_str) {
        assert(std::regex_match(map_str, regex_MAP_str()));
        bool reordered[512]{};
        auto put = [&reordered](int i, bool b) {
            if (i < 512) {
                reordered[i] = b;
            }
        };

        int chp = 3; // skip "MAP"
        int at = 0;
        while (at < 512) {
            uint8_t b6 = _impl_details::from_base64(map_str[chp++]);
            put(at + 5, (b6 >> 0) & 1);
            put(at + 4, (b6 >> 1) & 1);
            put(at + 3, (b6 >> 2) & 1);
            put(at + 2, (b6 >> 3) & 1);
            put(at + 1, (b6 >> 4) & 1);
            put(at + 0, (b6 >> 5) & 1);
            at += 6;
        }
        ruleT rule{};
        for (int code = 0; code < 512; ++code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            rule[code] = reordered[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1];
        }
        return rule;
    }
} // namespace legacy
