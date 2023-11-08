#pragma once

#include "partition.hpp"
#include "rule.hpp"

// TODO: rule generator and rule analyser/editor...
namespace legacy {
    // TODO: ud, lr, mdiag, sdiag...
    inline bool spatial_symmetric(const ruleT& rule) {
        return partition::get_partition_spatial().matches(rule.map);
    }

    inline bool center_agnostic_abs(const ruleT& rule) {
        for (int code = 0; code < 512; ++code) {
            int code_ns = code ^ (1 << 4); // s->!s
            if (rule[code] != rule[code_ns]) {
                return false;
            }
        }
        return true;
    }

    inline bool center_agnostic_xor(const ruleT& rule) {
        for (int code = 0; code < 512; ++code) {
            int code_ns = code ^ (1 << 4); // s->!s
            if ((decode_s(code) == rule[code]) != (decode_s(code_ns) == rule[code_ns])) {
                return false;
            }
        }
        return true;
    }

    inline bool state_symmetric(const ruleT& rule) {
        for (int code = 0; code < 512; ++code) {
            int code_n = (~code) & 511;
            if ((decode_s(code) == rule[code]) != (decode_s(code_n) == rule[code_n])) {
                return false;
            }
        }
        return true;
    }

    inline bool will_flick(const ruleT& rule) {
        return rule[0] == 1 && rule[511] == 0; // TODO: more robust encoding?
    }

    // TODO: how to incoporate this fact in the program?
    // spatial symmetry
    // state symm etry
    // state agnos cannot coexist together (due to spatica symmetric requirement)
    /*
    1. (if)
    111
    100 -> 0
    000

    1.->2. (agnostic)
    111
    110 -> 0
    000

    1.->3. (state sym)
    000
    011 -> 1
    111

    2.->4. (state sym)
    000
    001 -> 1
    111

    ok, but then ...
    4.->5. (spatial sym) conflicts with 1.
    111
    100 -> 1
    000
    */
} // namespace legacy
