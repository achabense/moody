// should be totally rewritten in the future.

#if 0
#include <random>

#include "rule.hpp"
#include "serialize.hpp"
#include "tile.hpp"

using namespace legacy;

namespace {
    void test_encode() {
        for (int i = 0; i < 512; i++) {
            assert(decode(i).encode() == i);
        }
    }

    void test_compress() {
        std::mt19937 rnd(uint32_t(time(0)));

        for (int i = 0; i < 10; ++i) {
            ruleT rule{};
            for (auto& b : rule) {
                b = rnd() & 1;
            }

            compress cmpr(rule);
            string str(cmpr);
            assert(rule == ruleT(cmpr));
            assert(cmpr == compress(str));
            assert(to_string(cmpr) == to_string(rule));
        }
    }

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

    // TODO: explain...
    consteval void test_symmetry() {
        int max = sym.map[0];
        for (auto i : sym.map) {
            if (i > max) {
                assert(i == max + 1);
                max = i;
            }
        }
    }

    void test_symmetry_2() {
        std::mt19937 rnd(uint32_t(time(0)));

        for (int i = 0; i < 10; ++i) {
            sruleT srule;
            srule.random_fill(sym.k / 2, rnd);

            constexpr int r = 40; // square, to make transform easy.
            tileT tile({r, r});
            tile.random_fill(0.5, rnd);
            auto check = [&](auto map_tile) {
                tileT A = map_tile(tile.gather().apply(srule));
                tileT B = map_tile(tile).gather().apply(srule);
                assert(A == B);
            };

            // TODO: why so many spaces?
#define CHECK(ymapped, xmapped)                                                                                        \
    check([](const tileT& tile) {                                                                                      \
        tileT mapped({r, r});                                                                                          \
        for (int y = 0; y < r; ++y) {                                                                                  \
            for (int x = 0; x < r; ++x) {                                                                              \
                mapped.at(ymapped, xmapped) = tile.at(y, x);                                                           \
            }                                                                                                          \
        }                                                                                                              \
        return mapped;                                                                                                 \
    });

            CHECK(y, x);
            CHECK(r - 1 - y, x);
            CHECK(y, r - 1 - x);
            CHECK(r - 1 - y, r - 1 - x);

            CHECK(x, y);
            CHECK(r - 1 - x, y);
            CHECK(x, r - 1 - y);
            CHECK(r - 1 - x, r - 1 - y);
        }
    } // namespace
} // namespace

namespace legacy {
    void run_tests() {
        test_encode();
        test_compress();
        test_symmetry();
        test_symmetry_2();
        puts("$");
    }
} // namespace legacy
#endif
