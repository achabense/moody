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

        constexpr int encode() const {
            // ~ bool is implicitly promoted to int.
            int code = (q << 0) | (w << 1) | (e << 2) |
                       (a << 3) | (s << 4) | (d << 5) |
                       (z << 6) | (x << 7) | (c << 8);
            assert(code >= 0 && code < 512);
            return code;
        }
    };
    // clang-format on

    constexpr envT decode(int code) {
        assert(code >= 0 && code < 512);
        bool q = (code >> 0) & 1, w = (code >> 1) & 1, e = (code >> 2) & 1;
        bool a = (code >> 3) & 1, s = (code >> 4) & 1, d = (code >> 5) & 1;
        bool z = (code >> 6) & 1, x = (code >> 7) & 1, c = (code >> 8) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    // Equivalent to envT(...).encode().
    constexpr int encode(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) {
        int code = (q << 0) | (w << 1) | (e << 2) | (a << 3) | (s << 4) | (d << 5) | (z << 6) | (x << 7) | (c << 8);
        assert(code >= 0 && code < 512);
        return code;
    }

    // Unambiguously refer to the map from env-code to the new state.
    // TODO: reconsider inheritance-based approach...
    // TODO: should allow implicit conversion?
    // TODO: recheck...

    // TODO: is it suitable to declare a namespace-enum for this?
    enum interpret_mode : bool { ABS, XOR };

    struct ruleT : public array<bool, 512> {
        using array_base = array<bool, 512>;

        constexpr ruleT() : array_base{} {}
        // TODO: better documentation, as xor or flip
        // TODO: explain; better logic structures...
        // TODO: from_base???
        constexpr ruleT(const array_base& base, interpret_mode mode) : array_base{base} {
            if (mode == XOR) {
                for (int code = 0; code < 512; ++code) {
                    bool s = (code >> 4) & 1; // TODO: helper-function...
                    operator[](code) = operator[](code) ? !s : s;
                }
            }
        }

        array_base to_base(interpret_mode mode) const {
            array_base r{*this};
            if (mode == XOR) {
                for (int code = 0; code < 512; ++code) {
                    bool s = (code >> 4) & 1; // TODO: helper-function...
                    r[code] = r[code] == s ? false : true;
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

    namespace _impl_details {
        struct symT {
            static const vector<vector<int>> groups;

            int k;               // Number of different groups.
            array<int, 512> map; // Map env code to the group it belongs to.
            consteval symT() {
                map.fill(-1);
                int color = 0;
                for (int code = 0; code < 512; code++) {
                    if (map[code] == -1) {
                        equiv(code, color++);
                    }
                }
                k = color;
            }

        private:
            // TODO: "color" might be confusing
            constexpr void equiv(int code, int color) {
                if (map[code] != -1) {
                    assert(map[code] == color);
                    return;
                }
                map[code] = color;
                // clang-format off
                auto [q, w, e,
                      a, s, d,
                      z, x, c] = decode(code);
                equiv(encode(z, x, c,
                             a, s, d,
                             q, w, e), color); // upside-down
                equiv(encode(e, w, q,
                             d, s, a,
                             c, x, z), color); // "leftside-right"
                equiv(encode(q, a, z,
                             w, s, x,
                             e, d, c), color); // diagonal
                // TODO: temporary...
                // equiv(encode(a, q, w,
                //              z, s, e,
                //              x, c, d), color); // rotate
                // TODO: temporary... how to incorporate this?
                equiv(encode(!q, !w, !e,
                             !a, !s, !d,
                             !z, !x, !c), color); // ?!
                // clang-format on
            }
        };

        inline constexpr symT sym;

        inline const vector<vector<int>> symT::groups = [] {
            vector<vector<int>> groups(sym.k);
            for (int code = 0; code < 512; ++code) {
                groups[sym.map[code]].push_back(code);
            }
            return groups;
        }();
    } // namespace _impl_details

    using _impl_details::sym;

    struct sruleT : public array<bool, sym.k> {
        using array_base = array<bool, sym.k>;

        constexpr sruleT() : array_base{} {}
        constexpr sruleT(const array_base& base) : array_base{base} {}

        // The implicit conversion interpret this as the map from env-code to the new state.
        /* implicit */ operator ruleT() const {
            ruleT rule{};
            for (int code = 0; code < 512; ++code) {
                rule[code] = at(sym.map[code]);
            }
            return rule;
        }

        // Interpret this as ???...TODO...
        ruleT as_flip() const {
            ruleT rule{};
            for (int code = 0; code < 512; ++code) {
                bool s = (code >> 4) & 1;
                rule[code] = at(sym.map[code]) ? !s : s;
            }
            return rule;
        }

        // provide minimal support for creating randomized rules.
        void random_fill(int ct, auto&& rnd) {
            ct = std::clamp(ct, 0, sym.k);
            fill(false);
            std::fill_n(begin(), ct, true);
            shuffle(rnd);
        }

        void shuffle(auto&& rnd) {
            std::shuffle(begin(), end(), rnd);
        }
    };
} // namespace legacy
