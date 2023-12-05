#pragma once

// "partition.hpp" v2. Should focus more on rule concepts.

#include <functional> // TODO: use std::function? is overhead large?
#include <map>
#include <span>

#include "rule.hpp"

namespace legacy {
    using ruleT_data = ruleT::data_type;

    using colorT = int;

    // TODO: rename...
    // vpartitionT denotes a partition of the set of all possible codeT ({0}~{511}).
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

    // Also called refinement...
    // TODO: refer to https://en.wikipedia.org/wiki/Partition_of_a_set#Refinement_of_partitions
    // Return the smallest partition that is ...
    // TODO: more formal name...
    vpartitionT lcm(const vpartitionT& a, const vpartitionT& b, int* groupc) {
        // int a;
        // int b;
        vpartitionT p{};
        p.fill(-1);
        auto flood = [&p, &a, &b](codeT code, int v, const auto& flood) -> void {
            if (p[code] == -1) {
                p[code] = v;
                for (codeT c : codeT{}) {
                    if (a[c] == a[code] || b[c] == b[code]) {
                        flood(c, v, flood);
                    }
                }
            }
        };

        int v = 0;
        for (codeT code : codeT{}) {
            if (p[code] == -1) {
                flood(code, v++, flood);
            }
        }
        if (groupc) {
            *groupc = v;
        }
        return p;
    }

    class wpartitionT {
        vpartitionT m_p{};
        int m_k{};
        std::array<codeT, 512> group_data{};
        std::vector<std::span<const codeT>> group_spans;

    public:
        colorT color_for(codeT code) { return m_p[code]; }
        std::span<const codeT> group_for(codeT code) { return group_spans[color_for(code)]; }
        codeT head_for(codeT code) { return group_for(code).front(); }

        operator const vpartitionT&() const { return m_p; }

        wpartitionT(const vpartitionT& p) : m_p(p) {
            m_k = regulate(m_p);
            make_groups();
        }
        // LCM.
        wpartitionT(const vpartitionT& a, const vpartitionT& b) {
            m_p = lcm(a, b, &m_k);
            make_groups();
        }

        //  TODO: this is also called that a is a refinement of b.
        bool subdivides(const vpartitionT& p) const {
            for (const auto& group : group_spans) {
                for (codeT code : group) {
                    if (p[code] != p[group[0]]) {
                        return false;
                    }
                }
            }
            return true;
        }

        // TODO: ...
        bool matches(const ruleT_data& r) {}

    private:
        void make_groups() {
            // Assert regulated...

            std::vector<int> count(m_k, 0);
            for (colorT col : m_p) {
                ++count[col];
            }

            std::vector<int> pos(m_k, 0);
            for (int j = 1; j < m_k; ++j) {
                pos[j] = pos[j - 1] + count[j - 1];
            }

            group_spans.resize(m_k);
            for (int j = 0; j < m_k; ++j) {
                group_spans[j] = {group_data.data() + pos[j], count[j]};
            }

            for (codeT code : codeT{}) {
                colorT col = m_p[code];
                group_data[pos[col]++] = code;
            }
        }
    };

    vpartitionT make1() {
        vpartitionT p{};
        for (codeT code : codeT{}) {
            p[code] = code;
        }
        return p;
    }

    // TODO: concept-based partitions...
    // Every well-defined concept should be supported by a class...
    // Such a class should be able to check and generate rules...

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

    using mapperT = codeT (*)(codeT);
#define MAPPER(...)                                      \
    +[](codeT code) {                                    \
        auto [q, w, e, a, s, d, z, x, c] = decode(code); \
        return encode(__VA_ARGS__);                      \
    }

    vpartitionT make_partition_chain(mapperT mapper, int* groupc = nullptr) {
        vpartitionT p{};
        p.fill(-1);
        auto flood = [&p, &mapper](codeT code, int v, const auto& flood) -> void {
            if (p[code] == -1) {
                p[code] = v;
                flood(mapper(code), v, flood);
            }
        };

        int v = 0;
        for (codeT code : codeT{}) {
            if (p[code] == -1) {
                flood(code, v++, flood);
            }
        }
        if (groupc) {
            *groupc = v;
        }
        return p;
    }

    struct independence {
        // TODO: what does it mean when saying a rule is independent of certain neighbors?
        static vpartitionT make_partition(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) {
            const codeT mask = encode(q, w, e, a, s, d, z, x, c);
            vpartitionT p{};
            for (codeT code : codeT{}) {
                p[code] = code & mask;
            }
            return p;
        }

        // TODO: not needed...
        inline static const wpartitionT q = make_partition(1, 0, 0, 0, 0, 0, 0, 0, 0);
        inline static const wpartitionT w = make_partition(0, 1, 0, 0, 0, 0, 0, 0, 0);
        inline static const wpartitionT e = make_partition(0, 0, 1, 0, 0, 0, 0, 0, 0);
        inline static const wpartitionT a = make_partition(0, 0, 0, 1, 0, 0, 0, 0, 0);
        inline static const wpartitionT s = make_partition(0, 0, 0, 0, 1, 0, 0, 0, 0);
        inline static const wpartitionT d = make_partition(0, 0, 0, 0, 0, 1, 0, 0, 0);
        inline static const wpartitionT z = make_partition(0, 0, 0, 0, 0, 0, 1, 0, 0);
        inline static const wpartitionT x = make_partition(0, 0, 0, 0, 0, 0, 0, 1, 0);
        inline static const wpartitionT c = make_partition(0, 0, 0, 0, 0, 0, 0, 0, 1);
    };

    // Recognizer and... [centered] on a cell...
    struct square_symmetry {
        enum effect {
            a, // -
            b, // |
            c, // \ (avoid ending with \)
            d, // /
            r180,
            r90
        };

        // z x c
        // a s d
        // q w e
        inline static const wpartitionT a_p = make_partition_chain(MAPPER(z, x, c, a, s, d, q, w, e));
        // e w q
        // d s a
        // c x z
        inline static const wpartitionT b_p = make_partition_chain(MAPPER(e, w, q, d, s, a, c, x, z));
        // q a z
        // w s x
        // e d c
        inline static const wpartitionT c_p = make_partition_chain(MAPPER(q, a, z, w, s, x, e, d, c));
        // c d e
        // x s w
        // z a q
        inline static const wpartitionT d_p = make_partition_chain(MAPPER(c, d, e, x, s, w, z, a, q));
        // TODO: is clockwise/counterclockwise significant here?
        // All clockwise, 270?
        // c x z
        // d s a
        // e w q
        inline static const wpartitionT r180_p = make_partition_chain(MAPPER(c, x, z, d, s, a, e, w, q));
        // w e d
        // q s c
        // a z x
        inline static const wpartitionT r90_p = make_partition_chain(MAPPER(w, e, d, q, s, c, a, z, x));

        using req = std::array<bool, 6>;
        wpartitionT make_partition(req r) {
            vpartitionT p = make1();
            if (r[0]) {
                p = lcm(p, a_p, nullptr);
            }
            if (r[1]) {
            }
            // ...
        }
        req test_partition(const wpartitionT& p) { //
            return {a_p.subdivides(p), b_p.subdivides(p), c_p.subdivides(p), d_p.subdivides(p) /*TODO*/};
        }
    };

    /*
      _/ \_
     / \_/ \
     \_/ \_/
     / \_/ \
     \_/ \_/
       \_/
    */
    struct hexagonal_symmetry {
        enum effect {
            a, // -
            b, // /
            c, // \ (avoid ending with \)
            ro180,
            ro120,
            ro60
        };
    };

    struct state_symmetry {};

    // All other partitions...
    struct unsystematic {};

    // TODO: and ... a partialT can be regarded as a constraint...
    // Detect whether a rule meets certain constraints.
    // A way to generate all the rules that can meet certain constraints.

    // ... in this stance a partition is not more special than a partialT?

    // TODO: how to correctly understand interT? is it even well-defined?
    // It's for sure that, an interT poses no constraint.

    // How to decide whether different constraints can coexit?
} // namespace legacy
