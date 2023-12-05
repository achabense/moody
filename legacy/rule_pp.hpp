#pragma once

// "partition.hpp" v2. Should focus more on rule concepts.

#include <map>
#include <span>

#include "rule.hpp"

namespace legacy {
    using colorT = int;

    // Map codeT to colorT.
    using vpartitionT = std::array<colorT, 512>;

    inline int regulate(vpartitionT& p) {
        std::map<colorT, colorT> regulator;
        colorT next = 0;
        for (colorT& col : p) {
            if (!regulator.contains(col)) {
                regulator[col] = next++;
            }
            col = regulator[col];
        }
        assert(next == regulator.size());
        return next;
    }

    inline bool regulated(const vpartitionT& p) {
        if (p[0] != 0) {
            return false;
        }
        for (colorT max = 0; colorT col : p) {
            if (max < col) {
                if (max + 1 != col) {
                    return false;
                }
                ++max;
            }
        }
        return true;
    }

    // or... (and is not needed...)

    class wpartitionT {
        vpartitionT m_p{};
        int k{};
        std::array<codeT, 512> q{};
        std::vector<std::span<const codeT>> groups;

    public:
        colorT color_for(codeT code) { return m_p[code]; }
        std::span<const codeT> group_for(codeT code) { return groups[color_for(code)]; }
        codeT head_for(codeT code) { return group_for(code).front(); }

        wpartitionT(const vpartitionT& p) : m_p(p) {
            k = regulate(m_p);
            std::array<int, 512> count{};
            for (colorT col : m_p) {
                ++count[col];
            }

            std::array<int, 512> start{};
            start[0] = 0;
            for (int j = 1; j < k; ++j) {
                start[j] = start[j - 1] + count[j - 1];
            }

            groups.resize(k);
            for (int j = 0; j < k; ++j) {
                groups[j] = {q.data() + start[j], count[j]};
            }

            // ...
            // TODO: scan...
            for (codeT code : codeT{}) {
                colorT col = m_p[code];
                q[start[col]++] = code;
            }
        }
    };

    // TODO: concept-based partitions...
    // Every well-defined concept should be supported by a class...
    // Such a class should be able to check and generate rules...

    // TODO: what does it mean when saying a rule is independent of certain neighbors?
    wpartitionT dependent_on(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) {
        const codeT mask = encode(q, w, e, a, s, d, z, x, c);
        vpartitionT p{};
        for (codeT code : codeT{}) {
            p[code] = code & mask;
        }
        return p;
    }

    // ud, lr, m, s... iso...

    // state...
    // when viewed in flip mode...
    wpartitionT statep() {
        vpartitionT p{};
        for (codeT code : codeT{}) {
            if (decode_s(code)) {
                p[code] = code;
            } else {
                p[code] = flip_all(code);
            }
        }
        return p;
    }

    // TODO: and ... a partialT can be regarded as a constraint...
    // Detect whether a rule meets certain constraints.
    // A way to generate all the rules that can meet certain constraints.

    // ... in this stance a partition is not more special than a partialT?

    // TODO: how to correctly understand interT? is it even well-defined?
    // It's for sure that, an interT poses no constraint.

    // How to decide whether different constraints can coexit?
} // namespace legacy
