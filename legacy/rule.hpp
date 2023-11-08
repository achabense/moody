#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <vector>

namespace legacy {
    using std::array;
    using std::string;
    using std::vector;

    // clang-format off
    // The environment around "s".
    struct envT {
        bool q, w, e;
        bool a, s, d;
        bool z, x, c;

        int encode() const {
            // ~ bool is implicitly promoted to int.
            int code = (q << 0) | (w << 1) | (e << 2) |
                       (a << 3) | (s << 4) | (d << 5) |
                       (z << 6) | (x << 7) | (c << 8);
            assert(code >= 0 && code < 512);
            return code;
        }
    };
    // clang-format on

    inline envT decode(int code) {
        assert(code >= 0 && code < 512);
        bool q = (code >> 0) & 1, w = (code >> 1) & 1, e = (code >> 2) & 1;
        bool a = (code >> 3) & 1, s = (code >> 4) & 1, d = (code >> 5) & 1;
        bool z = (code >> 6) & 1, x = (code >> 7) & 1, c = (code >> 8) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    // Equivalent to decode(code).s.
    inline bool decode_s(int code) {
        return (code >> 4) & 1;
    }

    // Equivalent to envT(...).encode().
    inline int encode(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) {
        int code = (q << 0) | (w << 1) | (e << 2) | (a << 3) | (s << 4) | (d << 5) | (z << 6) | (x << 7) | (c << 8);
        assert(code >= 0 && code < 512);
        return code;
    }

    // TODO: rephrase...
    // Unambiguously refer to the map from env-code to the new state.
    struct ruleT {
        using data_type = array<bool, 512>;

        data_type map{}; // mapping of s->s'.

        bool operator()(int code) const {
            assert(code >= 0 && code < 512);
            return map[code];
        }

        bool operator()(const envT& env) const {
            return map[env.encode()];
        }

        bool operator()(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) const {
            return map[encode(q, w, e, a, s, d, z, x, c)];
        }

        // Conceptually only for data access.
        const bool& operator[](int at) const {
            return map[at];
        }

        bool& operator[](int at) {
            return map[at];
        }

        friend bool operator==(const ruleT&, const ruleT&) = default;
    };

    // "Convay's Game of Life" rule.
    inline ruleT game_of_life() {
        // b3 s23
        ruleT rule{};
        for (int code = 0; code < 512; ++code) {
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

    using ruleT_data = ruleT::data_type;

    // TODO: explain...
    inline ruleT_data to_flip(const ruleT& rule) {
        ruleT_data flip{};
        for (int code = 0; code < 512; ++code) {
            flip[code] = rule[code] == decode_s(code) ? false : true;
        }
        return flip;
    }

    inline ruleT from_flip(const ruleT_data& flip) {
        ruleT rule{};
        for (int code = 0; code < 512; ++code) {
            bool s = decode_s(code);
            rule[code] = flip[code] ? !s : s;
        }
        return rule;
    }

    // TODO: is it suitable to declare a namespace-enum for this?
    enum interpret_mode : int { ABS = false, XOR = true };

    inline ruleT_data from_rule(const ruleT& rule, interpret_mode interp) {
        return interp == ABS ? rule.map : to_flip(rule);
    }
    inline ruleT to_rule(const ruleT_data& rule_data, interpret_mode interp) {
        return interp == ABS ? ruleT{rule_data} : from_flip(rule_data);
    }

    class compressT {
        array<uint8_t, 64> bits; // as bitset.
    public:
        explicit compressT(const ruleT& rule) : bits{} {
            for (int code = 0; code < 512; ++code) {
                bits[code / 8] |= rule[code] << (code % 8);
            }
        }

        explicit operator ruleT() const {
            ruleT rule{};
            for (int code = 0; code < 512; ++code) {
                rule[code] = (bits[code / 8] >> (code % 8)) & 1;
            }
            return rule;
        }

        friend bool operator==(const compressT& l, const compressT& r) = default;

        friend bool operator<(const compressT& l, const compressT& r) = default;

        // TODO: hashing...
    };
} // namespace legacy
