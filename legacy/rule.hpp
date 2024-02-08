#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <regex>
#include <span>
#include <string>
#include <vector>

// TODO: recheck captures in the form of [&]...
// TODO: tell apart precondition and impl assertion...

// TODO: remove the `T` suffix? initially they were for things like `codeT code`...
// TODO: bool might not be a good idea...
// For example, bool is allowed to have padding bits, so memcmp may not apply...

namespace legacy {
    // The environment around `s`.
    // (The variables are named after the keys in the qwerty keyboard.)
    struct envT {
        bool q, w, e;
        bool a, s, d;
        bool z, x, c;
    };

    // TODO: remove remaining direct use of "512"...
    // Encode envT to an integer.
    struct codeT {
        int val;
        constexpr /*implicit*/ operator int() const {
            assert(val >= 0 && val < 512);
            return val;
        }

        template <class T>
        class map_to {
            std::array<T, 512> m_map{};

        public:
            constexpr const T& operator[](codeT code) const { return m_map[code]; }
            constexpr T& operator[](codeT code) { return m_map[code]; }

            constexpr void fill(const T& val) { m_map.fill(val); }
            constexpr friend bool operator==(const map_to&, const map_to&) = default;
        };
#
        // TODO: rename to bpos_*?
        enum bposE : int { env_q = 0, env_w, env_e, env_a, env_s, env_d, env_z, env_x, env_c };
    };

    // (`name` must not be modified within the loop body)
#define for_each_code(name) for (::legacy::codeT name{.val = 0}; name.val < 512; ++name.val)

    constexpr codeT encode(const envT& env) {
        // ~ bool is implicitly promoted to int.
        // clang-format off
        using enum codeT::bposE;
        const int code = (env.q << env_q) | (env.w << env_w) | (env.e << env_e) |
                         (env.a << env_a) | (env.s << env_s) | (env.d << env_d) |
                         (env.z << env_z) | (env.x << env_x) | (env.c << env_c);
        // clang-format on
        assert(code >= 0 && code < 512);
        return codeT{code};
    }

    constexpr envT decode(codeT code) {
        using enum codeT::bposE;
        const bool q = (code >> env_q) & 1, w = (code >> env_w) & 1, e = (code >> env_e) & 1;
        const bool a = (code >> env_a) & 1, s = (code >> env_s) & 1, d = (code >> env_d) & 1;
        const bool z = (code >> env_z) & 1, x = (code >> env_x) & 1, c = (code >> env_c) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    constexpr bool get(codeT code, codeT::bposE bpos) { //
        return (code >> bpos) & 1;
    }

    constexpr bool get_s(codeT code) { //
        return (code >> codeT::env_s) & 1;
    }

#ifndef NDEBUG
    namespace _misc::tests {
        inline const bool test_codeT = [] {
            for_each_code(code) { assert(encode(decode(code)) == code); }
            return true;
        }();
    } // namespace _misc::tests
#endif

    // Map codeT to the value `s` become at next generation.
    class ruleT {
        codeT::map_to<bool> m_map{};

    public:
        constexpr bool operator()(codeT code) const { return m_map[code]; }

        constexpr bool operator[](codeT code) const { return m_map[code]; }
        constexpr bool& operator[](codeT code) { return m_map[code]; }

        constexpr friend bool operator==(const ruleT&, const ruleT&) = default;
    };

    // TODO: rename...
    // TODO: Document that this is not the only situation that flicking effect can occur...
    inline bool will_flick(const ruleT& rule) {
        constexpr codeT all_0 = encode({0, 0, 0, 0, 0, 0, 0, 0, 0});
        constexpr codeT all_1 = encode({1, 1, 1, 1, 1, 1, 1, 1, 1});
        return rule[all_0] == 1 && rule[all_1] == 0;
    }

    inline constexpr ruleT make_rule(const auto& fn) {
        ruleT rule{};
        for_each_code(code) { rule[code] = fn(code); }
        return rule;
    }

    // "Convay's Game of Life" (B3/S23)
    inline ruleT game_of_life() {
        return make_rule([](codeT code) -> bool {
            const auto [q, w, e, a, s, d, z, x, c] = decode(code);
            const int count = q + w + e + a + d + z + x + c;
            if (count == 2) { // 2:S ~ 0->0, 1->1 ~ equal to "s".
                return s;
            } else if (count == 3) { // 3:BS ~ 0->1, 1->1 ~ always 1.
                return 1;
            } else {
                return 0;
            }
        });
    }

    // TODO: explain...
    struct moldT {
        using lockT = codeT::map_to<bool>;

        ruleT rule{};
        lockT lock{};

        friend bool operator==(const moldT&, const moldT&) = default;
    };

    // TODO: rename...
    // TODO: add test...
    struct compressT {
        std::array<uint8_t, 64> bits_rule{}, bits_lock{}; // as bitset.

        friend bool operator==(const compressT&, const compressT&) = default;
        struct hashT {
            size_t operator()(const compressT& cmpr) const {
                // ~ not UB.
                return std::hash<std::string_view>{}(
                    std::string_view(reinterpret_cast<const char*>(&cmpr), sizeof(cmpr)));
            }
        };
    };

    inline compressT compress(const moldT& mold) {
        compressT cmpr{};
        for_each_code(code) {
            cmpr.bits_rule[code / 8] |= mold.rule[code] << (code % 8);
            cmpr.bits_lock[code / 8] |= mold.lock[code] << (code % 8);
        }
        return cmpr;
    }

    inline moldT decompress(const compressT& cmpr) {
        moldT mold{};
        for_each_code(code) {
            mold.rule[code] = (cmpr.bits_rule[code / 8] >> (code % 8)) & 1;
            mold.lock[code] = (cmpr.bits_lock[code / 8] >> (code % 8)) & 1;
        }
        return mold;
    }

    namespace _misc {
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
                assert(ch == '/');
                return 63;
            }
        }

        // TODO: refine impl...
        // TODO: explain the encoding scheme... (about `data`)
        inline void append_base64(std::string& str, const auto& source /* ruleT or lockT */) {
            bool data[512]{}; // Re-encoded as if bpos_q = 8, ... bpos_c = 0.
            for_each_code(code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                data[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1] = source[code];
            }
            const auto get = [&data](int i) { return i < 512 ? data[i] : 0; };
            for (int i = 0; i < 512; i += 6) {
                const uint8_t b6 = (get(i + 5) << 0) | (get(i + 4) << 1) | (get(i + 3) << 2) | (get(i + 2) << 3) |
                                   (get(i + 1) << 4) | (get(i + 0) << 5);
                str += to_base64(b6);
            }
        }

        inline void load_base64(std::string_view str, auto& dest /* ruleT or lockT */) {
            bool data[512]{}; // Re-encoded as if bpos_q = 8, ... bpos_c = 0.
            auto put = [&data](int i, bool b) {
                if (i < 512) {
                    data[i] = b;
                }
            };

            int chp = 0;
            for (int i = 0; i < 512; i += 6) {
                const uint8_t b6 = from_base64(str[chp++]);
                put(i + 5, (b6 >> 0) & 1);
                put(i + 4, (b6 >> 1) & 1);
                put(i + 3, (b6 >> 2) & 1);
                put(i + 2, (b6 >> 3) & 1);
                put(i + 1, (b6 >> 4) & 1);
                put(i + 0, (b6 >> 5) & 1);
            }

            for_each_code(code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                dest[code] = data[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1];
            }
        }
    } // namespace _misc

    // Convert ruleT to an "MAP rule" string.
    inline std::string to_MAP_str(const ruleT& rule) {
        std::string str = "MAP";
        _misc::append_base64(str, rule);
        return str;
    }

    inline std::string to_MAP_str(const moldT& mold) {
        std::string str = "MAP";
        _misc::append_base64(str, mold.rule);
        str += " [";
        _misc::append_base64(str, mold.lock);
        str += "]";
        return str;
    }

    struct extrT {
        std::optional<moldT> mold{};
        std::string_view prefix{}, suffix{};
    };

    // TODO: ?? unpaired rules tend to mean the locks are unknown/not cared about. In these cases it might
    // b better to try to fit the rule into current mold, and break lock if not fitting...

    // TODO: or return optional<{ruleT,optional<lockT>}>?
    // Extract moldT from text. Rules unpaired with lock data are treated as moldT with empty lock.
    inline extrT extract_MAP_str(const char* begin, const char* end) {
        extrT extr{};

        static_assert((512 + 5) / 6 == 86);
        //                                1                  2       3
        static const std::regex regex{"MAP([a-zA-Z0-9+/]{86})( \\[([a-zA-Z0-9+/]{86})\\])?",
                                      std::regex_constants::optimize};

        if (std::cmatch match; std::regex_search(begin, end, match, regex)) {
            extr.prefix = {match.prefix().first, match.prefix().second};
            extr.suffix = {match.suffix().first, match.suffix().second};

            ruleT rule{};
            _misc::load_base64({match[1].first, match[1].second}, rule);
            extr.mold.emplace(rule);
            if (match[3].matched) {
                _misc::load_base64({match[3].first, match[3].second}, extr.mold->lock);
            }
        } else {
            extr.prefix = {begin, end};
            extr.suffix = {};
        }

        return extr;
    }

    inline extrT extract_MAP_str(std::span<const char> data) {
        return extract_MAP_str(data.data(), data.data() + data.size());
    }

#ifndef NDEBUG
    // TODO: extend test coverage to affixed/with-lock cases etc...
    namespace _misc::tests {
        // https://golly.sourceforge.io/Help/Algorithms/QuickLife.html
        // > So, Conway's Life (B3/S23) encoded as a MAP rule is:
        // > rule = MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
        inline const bool test_MAP_str = [] {
            const std::string_view gol_str =
                "MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA";
            const ruleT gol = game_of_life();
            assert(to_MAP_str(gol) == gol_str);

            const auto [mold, prefix, suffix] = extract_MAP_str(gol_str);
            assert(prefix == "");
            assert(suffix == "");
            assert(mold);
            assert(mold->rule == gol);
            assert(mold->lock == moldT::lockT{});

            return true;
        }();
    } // namespace _misc::tests
#endif

} // namespace legacy
