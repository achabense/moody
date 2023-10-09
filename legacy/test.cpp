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

    // TODO: test that identity doesn't ...
    inline constexpr ruleT rule_identity = [] {
        ruleT rule{};
        for (int code = 0; code < 512; ++code) {
            bool s = (code >> 4) & 1;
            rule[code] = s;
        }
        return rule;
    }();

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
