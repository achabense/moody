#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <random>
#include <regex>
#include <span>
#include <string>
#include <vector>

// TODO: refine tests...
#ifndef NDEBUG
#define ENABLE_TESTS
#endif // !NDEBUG

// TODO: recheck c-style casts...
// TODO: recheck captures in the form of [&]...
// TODO: tell apart precondition and impl assertion...

static_assert(INT_MAX >= INT32_MAX);

namespace legacy {

#ifdef ENABLE_TESTS
    namespace _tests {
        struct testT {
            inline static std::mt19937 rand{(uint32_t)time(0)};
            testT(const auto& fn) noexcept { fn(); }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    // TODO: make the state of the cell strongly-typed (and maybe not use bool as underlying type)?
    // (This will entails a lot of casts but will make the concepts clearer)
    // enum class cellT : bool {};

    // The state of `s` and its neighbors.
    // (The variables are named after the keys in the qwerty keyboard.)
    struct situT {
        bool q, w, e;
        bool a, s, d;
        bool z, x, c;
    };

    // TODO: remove remaining direct use of "512"...
    // `situT` encoded as an integer.
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
        enum bposE : int { bpos_q = 0, bpos_w, bpos_e, bpos_a, bpos_s, bpos_d, bpos_z, bpos_x, bpos_c };
    };

    constexpr codeT encode(const situT& situ) {
        // ~ bool is implicitly promoted to int.
        using enum codeT::bposE;
        const int code = (situ.q << bpos_q) | (situ.w << bpos_w) | (situ.e << bpos_e) | //
                         (situ.a << bpos_a) | (situ.s << bpos_s) | (situ.d << bpos_d) | //
                         (situ.z << bpos_z) | (situ.x << bpos_x) | (situ.c << bpos_c);
        assert(code >= 0 && code < 512);
        return codeT{code};
    }

    constexpr situT decode(codeT code) {
        using enum codeT::bposE;
        const bool q = (code >> bpos_q) & 1, w = (code >> bpos_w) & 1, e = (code >> bpos_e) & 1;
        const bool a = (code >> bpos_a) & 1, s = (code >> bpos_s) & 1, d = (code >> bpos_d) & 1;
        const bool z = (code >> bpos_z) & 1, x = (code >> bpos_x) & 1, c = (code >> bpos_c) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    constexpr void for_each_code(const auto& fn) {
        for (codeT code{.val = 0}; code.val < 512; ++code.val) {
            fn(codeT(code));
        }
    }

    constexpr bool for_each_code_all_of(const auto& pred) {
        for (codeT code{.val = 0}; code.val < 512; ++code.val) {
            if (!pred(codeT(code))) {
                return false;
            }
        }
        return true;
    }

    constexpr bool get(codeT code, codeT::bposE bpos) { //
        return (code >> bpos) & 1;
    }

    constexpr bool get_s(codeT code) { //
        return (code >> codeT::bpos_s) & 1;
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_codeT = [] { //
            for_each_code([](codeT code) { assert(encode(decode(code)) == code); });
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    // Map `codeT` to the value `s` become at next generation.
    class ruleT {
        codeT::map_to<bool> m_map{};

    public:
        constexpr bool operator()(codeT code) const { return m_map[code]; }

        constexpr bool operator[](codeT code) const { return m_map[code]; }
        constexpr bool& operator[](codeT code) { return m_map[code]; }

        constexpr friend bool operator==(const ruleT&, const ruleT&) = default;
    };

    template <class T>
    concept rule_like = std::is_invocable_r_v<bool, T, codeT>;

    static_assert(rule_like<ruleT>);

    // TODO: rename...
    // TODO: Document that this is not the only situation that flicking effect can occur...
    inline bool will_flick(const ruleT& rule) {
        constexpr codeT all_0 = encode({0, 0, 0, 0, 0, 0, 0, 0, 0});
        constexpr codeT all_1 = encode({1, 1, 1, 1, 1, 1, 1, 1, 1});
        return rule[all_0] == 1 && rule[all_1] == 0;
    }

    constexpr ruleT make_rule(const rule_like auto& fn) {
        ruleT rule{};
        for_each_code([&](codeT code) { rule[code] = fn(code); });
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
        for_each_code([&](codeT code) {
            cmpr.bits_rule[code / 8] |= mold.rule[code] << (code % 8);
            cmpr.bits_lock[code / 8] |= mold.lock[code] << (code % 8);
        });
        return cmpr;
    }

    inline moldT decompress(const compressT& cmpr) {
        moldT mold{};
        for_each_code([&](codeT code) {
            mold.rule[code] = (cmpr.bits_rule[code / 8] >> (code % 8)) & 1;
            mold.lock[code] = (cmpr.bits_lock[code / 8] >> (code % 8)) & 1;
        });
        return mold;
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_compressT = [] {
            moldT mold{};
            for_each_code([&](codeT code) {
                mold.rule[code] = testT::rand() & 1;
                mold.lock[code] = testT::rand() & 1;
            });
            assert(decompress(compress(mold)) == mold);
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

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
            for_each_code([&](codeT code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                data[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1] = source[code];
            });
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

            for_each_code([&](codeT code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                dest[code] = data[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1];
            });
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
        // Acts as a variant for ruleT/moldT.
        struct valT {
            ruleT rule;
            std::optional<moldT::lockT> lock;
        };

        std::optional<valT> val;
        std::string_view prefix{}, suffix{};
    };

    // Extract ruleT or moldT from text.
    inline extrT extract_MAP_str(const char* begin, const char* end) {
        extrT extr{};

        static_assert((512 + 5) / 6 == 86);
        //                                1                  2       3
        static const std::regex regex{"MAP([a-zA-Z0-9+/]{86})( \\[([a-zA-Z0-9+/]{86})\\])?",
                                      std::regex_constants::optimize};

        if (std::cmatch match; std::regex_search(begin, end, match, regex)) {
            extr.prefix = {match.prefix().first, match.prefix().second};
            extr.suffix = {match.suffix().first, match.suffix().second};

            _misc::load_base64({match[1].first, match[1].second}, extr.val.emplace().rule);
            if (match[3].matched) {
                _misc::load_base64({match[3].first, match[3].second}, extr.val->lock.emplace());
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

#ifdef ENABLE_TESTS
    namespace _tests {
        // https://golly.sourceforge.io/Help/Algorithms/QuickLife.html
        // > So, Conway's Life (B3/S23) encoded as a MAP rule is:
        // > rule = MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
        inline const testT test_MAP_str = [] {
            {
                const std::string_view str = "...";
                const auto [val, prefix, suffix] = extract_MAP_str(str);
                assert(!val);
                assert(prefix == "...");
                assert(suffix == "");
            }

            {
                const std::string_view gol_str =
                    "MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA";
                const ruleT gol = game_of_life();
                assert(to_MAP_str(gol) == gol_str);

                const auto [val, prefix, suffix] = extract_MAP_str(gol_str);
                assert(prefix == "");
                assert(suffix == "");
                assert(val);
                assert(val->rule == gol);
                assert(!val->lock);
            }

            {
                moldT mold{};
                for_each_code([&](codeT code) {
                    mold.rule[code] = testT::rand() & 1;
                    mold.lock[code] = testT::rand() & 1;
                });
                const std::string rule_only = "(prefix)" + to_MAP_str(mold.rule) + "(suffix)";
                const std::string whole = "(prefix)" + to_MAP_str(mold) + "(suffix)";

                const auto [val1, prefix1, suffix1] = extract_MAP_str(rule_only);
                const auto [val2, prefix2, suffix2] = extract_MAP_str(whole);

                assert(prefix1 == "(prefix)" && prefix2 == "(prefix)");
                assert(suffix1 == "(suffix)" && suffix2 == "(suffix)");
                assert(val1 && val2);
                assert(val1->rule == mold.rule);
                assert(!val1->lock);
                assert(val2->rule == mold.rule);
                assert(val2->lock == mold.lock);
            }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

} // namespace legacy
