#pragma once

#include "partition.hpp"
#include "rule.hpp"

// TODO: merge into partition.hpp.
// TODO: rule generator and rule analyser/editor...
namespace legacy {
    // TODO: ud, lr, mdiag, sdiag...
    inline bool spatial_symmetric(const ruleT& rule) {
        return partitionT::get_spatial().matches(rule.map);
    }

    inline bool center_agnostic_abs(const ruleT& rule) {
        for (codeT code : codeT{}) {
            if (rule(code) != rule(flip_s(code))) {
                return false;
            }
        }
        return true;
    }

    inline bool center_agnostic_xor(const ruleT& rule) {
        for (codeT code : codeT{}) {
            codeT code_ns = flip_s(code);
            if ((decode_s(code) == rule(code)) != (decode_s(code_ns) == rule(code_ns))) {
                return false;
            }
        }
        return true;
    }

    inline bool state_symmetric(const ruleT& rule) {
        for (codeT code : codeT{}) {
            codeT codex = flip_all(code);
            if ((decode_s(code) == rule(code)) != (decode_s(codex) == rule(codex))) {
                return false;
            }
        }
        return true;
    }

    inline bool will_flick(const ruleT& rule) {
        // TODO: adhoc...
        return rule(codeT{0}) == 1 && rule(codeT{511}) == 0; // TODO: more robust encoding?
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
