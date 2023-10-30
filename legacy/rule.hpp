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

    // TOOD: reconsider using bool[3][3]?
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

    // TODO: reword...
    // Unambiguously refer to the map from env-code to the new state.
    // TODO: reconsider inheritance-based approach...
    // TODO: should allow implicit conversion?
    // TODO: recheck...
    // TODO: should be read-only after construction?
    // TODO: make from_base, to_base, global functions?

    // TODO: is it suitable to declare a namespace-enum for this?
    enum interpret_mode : int { ABS = false, XOR = true };

    struct ruleT : public array<bool, 512> {
        using array_base = array<bool, 512>;

        ruleT() : array_base{} {}
        // TODO: better documentation, as xor or flip
        // TODO: explain; better logic structures...
        // TODO: from_base???
        ruleT(const array_base& base, interpret_mode mode) : array_base{base} {
            if (mode == XOR) {
                for (int code = 0; code < 512; ++code) {
                    bool s = decode_s(code);
                    operator[](code) = operator[](code) ? !s : s;
                }
            }
        }

        array_base to_base(interpret_mode mode) const {
            array_base r{*this};
            if (mode == XOR) {
                for (int code = 0; code < 512; ++code) {
                    r[code] = r[code] == decode_s(code) ? false : true;
                }
            }
            return r;
        }

        bool map(int code) const {
            assert(code >= 0 && code < 512);
            return operator[](code);
        }

        bool map(const envT& env) const {
            return map(env.encode());
        }

        bool map(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) const {
            return map(encode(q, w, e, a, s, d, z, x, c));
        }
    };

    // "Convay's Game of Life" rule.
    inline ruleT game_of_life() {
        // b3 s23
        ruleT rule{};
        for (int code = 0; code < 512; ++code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            int count = q + w + e + a + d + z + x + c;
            if (count == 2) { // 2:s ~ 0->0, 1->1 ~ equal to "s".
                rule[code] = s;
            } else if (count == 3) { // 3:bs ~ 0->1, 1->1 ~ always 1.
                rule[code] = 1;
            } else {
                rule[code] = 0;
            }
        }
        return rule;
    }

    using ruleT_base = ruleT::array_base;
    inline ruleT_base from_rule(const ruleT& rule, interpret_mode interp) {
        return rule.to_base(interp);
    }
    inline ruleT to_rule(const ruleT_base& rule_data, interpret_mode interp) {
        return ruleT(rule_data, interp);
    }

} // namespace legacy
