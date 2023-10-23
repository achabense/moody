#include "rule.hpp"
#include "serialize.hpp"

using namespace legacy;

bool is_gol_str(const string& str) {
    try {
        auto rule = from_string<ruleT>(str);
        for (int code = 0; code < 512; ++code) {
            auto [q, w, e, a, s, d, z, x, c] = decode(code);
            int count = q + w + e + a + d + z + x + c;
            bool expected;
            if (count == 2) {
                expected = s;
            } else if (count == 3) {
                expected = true;
            } else {
                expected = false;
            }
            if (rule[code] != expected) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}
