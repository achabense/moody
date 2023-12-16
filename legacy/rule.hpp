#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <regex>
#include <string>
#include <vector>

// TODO: bool might not be a good idea...
// For example, bool is allowed to have padding bits, so memcmp may not apply...

namespace legacy {
    // The environment around "s".
    // TODO: how widespread is `qwerty` keyboard? without it this notation will pose no benefit...
    struct envT {
        bool q, w, e;
        bool a, s, d;
        bool z, x, c;
    };

    // TODO: remove the `T` suffix? initially this was for things like `codeT code`...
    // TODO: remove remaining direct use of "512"...
    struct codeT {
        int v;
        constexpr /*implicit*/ operator int() const {
            assert(v >= 0 && v < 512);
            return v;
        }

        // constexpr friend bool operator==(const codeT&, const codeT&) = default;

        // TODO: apply this in the future...
        template <class T>
        using map_to = std::array<T, 512>;
    };

    // TODO: better name? for_each_situ?
    // ~ should be name.v < 512
    // This has been proven better than defining a bunch of code for range-for loop.
#define for_each_code(name) for (::legacy::codeT name{.v = 0}; name.v < 512; ++name.v)

    // TODO: `encode` and `decode` are strictly subutils for codeT...
    constexpr codeT encode(const envT& env) {
        // ~ bool is implicitly promoted to int.
        // clang-format off
        int code = (env.q << 0) | (env.w << 1) | (env.e << 2) |
                   (env.a << 3) | (env.s << 4) | (env.d << 5) |
                   (env.z << 6) | (env.x << 7) | (env.c << 8);
        // clang-format on
        assert(code >= 0 && code < 512);
        return codeT{code};
    }

    // Equivalent to encode(envT(...)).
    constexpr codeT encode(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) {
        int code = (q << 0) | (w << 1) | (e << 2) | (a << 3) | (s << 4) | (d << 5) | (z << 6) | (x << 7) | (c << 8);
        assert(code >= 0 && code < 512);
        return codeT{code};
    }

    constexpr envT decode(codeT code) {
        assert(code >= 0 && code < 512);
        bool q = (code >> 0) & 1, w = (code >> 1) & 1, e = (code >> 2) & 1;
        bool a = (code >> 3) & 1, s = (code >> 4) & 1, d = (code >> 5) & 1;
        bool z = (code >> 6) & 1, x = (code >> 7) & 1, c = (code >> 8) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    // TODO: plan: add basic tests in debug mode.
    // should these functions be guarded by NDEBUG, or a project-defined macro?
#ifndef NDEBUG
    inline void test_codeT() {
        for_each_code(code) {
            const envT e = decode(code);
            if (encode(e) != code) {
                abort();
            }
        }
    }
#endif

    // TODO: better names...
    // TODO: whether still useful after applying rule_pp header?
    constexpr bool decode_s(codeT code) {
        return (code >> 4) & 1;
    }

    constexpr codeT flip_all(codeT code) {
        return codeT{~code & 511};
    }

    // TODO: better rename to something independent of ruleT...
    using ruleT_data = codeT::map_to<bool>;

    // TODO: rephrase...
    // Unambiguously refer to the map from env-code to the new state.
    struct ruleT {
        ruleT_data map{};

        bool operator()(codeT code) const { return map[code]; }
        bool operator()(const envT& env) const { //
            return map[encode(env)];
        }
        bool operator()(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) const { //
            return map[encode(q, w, e, a, s, d, z, x, c)];
        }

        friend bool operator==(const ruleT&, const ruleT&) = default;
    };

    // "Convay's Game of Life" rule.
    inline ruleT game_of_life() {
        // b3 s23
        ruleT rule{};
        for_each_code(code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            int count = q + w + e + a + d + z + x + c;
            if (count == 2) { // 2:s ~ 0->0, 1->1 ~ equal to "s".
                rule.map[code] = s;
            } else if (count == 3) { // 3:bs ~ 0->1, 1->1 ~ always 1.
                rule.map[code] = 1;
            } else {
                rule.map[code] = 0;
            }
        }
        return rule;
    }

    // TODO: is this actually useful in the application? even if recorder is needed, is plain ruleT not affordable?
    class compressT {
        std::array<uint8_t, 64> bits; // as bitset.
    public:
        explicit compressT(const ruleT& rule) : bits{} {
            for_each_code(code) {
                bits[code / 8] |= rule(code) << (code % 8);
            }
        }

        /*implicit*/ operator ruleT() const {
            ruleT rule{};
            for_each_code(code) {
                rule.map[code] = (bits[code / 8] >> (code % 8)) & 1;
            }
            return rule;
        }

        friend bool operator==(const compressT& l, const compressT& r) = default;

        struct hashT {
            size_t operator()(const compressT& cmpr) const {
                // ~ not UB.
                const char* data = reinterpret_cast<const char*>(cmpr.bits.data());
                return std::hash<std::string_view>{}(std::string_view(data, 64));
            }
        };
    };
} // namespace legacy

// TODO: (together with pattern copy/pasting) move to a separate header...
// TODO: talk about utf8 compatibility...
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
    inline std::string to_MAP_str(const ruleT& rule) {
        // MAP rule uses a different coding scheme.
        bool reordered[512]{};
        for_each_code(code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            reordered[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1] = rule(code);
        }
        auto get = [&reordered](int i) { return i < 512 ? reordered[i] : 0; };
        std::string str = "MAP";
        int i = 0;
        while (i < 512) {
            uint8_t b6 = (get(i + 5) << 0) | (get(i + 4) << 1) | (get(i + 3) << 2) | (get(i + 2) << 3) |
                         (get(i + 1) << 4) | (get(i + 0) << 5);
            str += _impl_details::to_base64(b6);
            i += 6;
        }
        return str;
    }

    inline const std::regex& regex_MAP_str() {
        static_assert((512 + 5) / 6 == 86);
        static std::regex reg{"MAP[a-zA-Z0-9+/]{86}", std::regex_constants::optimize};
        return reg;
    }

    inline ruleT from_MAP_str(const std::string& map_str) {
        assert(std::regex_match(map_str, regex_MAP_str()));
        bool reordered[512]{};
        auto put = [&reordered](int i, bool b) {
            if (i < 512) {
                reordered[i] = b;
            }
        };

        int chp = 3; // skip "MAP"
        int i = 0;
        while (i < 512) {
            uint8_t b6 = _impl_details::from_base64(map_str[chp++]);
            put(i + 5, (b6 >> 0) & 1);
            put(i + 4, (b6 >> 1) & 1);
            put(i + 3, (b6 >> 2) & 1);
            put(i + 2, (b6 >> 3) & 1);
            put(i + 1, (b6 >> 4) & 1);
            put(i + 0, (b6 >> 5) & 1);
            i += 6;
        }
        ruleT rule{};
        for_each_code(code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            rule.map[code] = reordered[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1];
        }
        return rule;
    }

#ifndef NDEBUG
    // Source: https://golly.sourceforge.io/Help/Algorithms/QuickLife.html
    // > So, Conway's Life (B3/S23) encoded as a MAP rule is:
    // > rule = MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
    inline void test_MAP_str() {
        const char* gol_str =
            "MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA";
        const ruleT gol = game_of_life();
        if (!std::regex_match(gol_str, regex_MAP_str())) {
            abort();
        }
        if (gol_str != to_MAP_str(gol)) {
            abort();
        }
        if (from_MAP_str(gol_str) != gol) {
            abort();
        }
    }
#endif

} // namespace legacy
