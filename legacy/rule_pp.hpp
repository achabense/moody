#pragma once

// "partition.hpp" v2. Should focus more on rule concepts.

#include <functional> // TODO: use std::function? is overhead large?
#include <map>
#include <span>

#include "rule.hpp"

namespace legacy {
    // TODO: rename / explain...
    struct partialT {
        enum stateE : char { S0, S1, Unknown };
        std::array<stateE, 512> map;
        void reset() { map.fill(Unknown); }
        partialT() { reset(); }

        // Conflicts are not allowed
        void set(codeT code, bool b) {
            assert(map[code] == Unknown || map[code] == b);
            map[code] = stateE{b};
        }

        // TODO: sometimes need to listen to selected area...
        auto bind(const ruleT& rule) {
            return [this, rule](codeT code) /*const*/ {
                set(code, rule(code));
                return rule(code);
            };
        }
    };

    // TODO: when to introduce "interT"?
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
        colorT color_for(codeT code) const { return m_p[code]; }
        std::span<const codeT> group_for(codeT code) const { return group_spans[color_for(code)]; }
        codeT head_for(codeT code) const { return group_for(code).front(); }

        operator const vpartitionT&() const { return m_p; }

        wpartitionT(const vpartitionT& p) : m_p(p) {
            m_k = regulate(m_p);

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

        // As `group_spans` refer to data in group_data, this is not directly copyable.
        wpartitionT(const wpartitionT&) = delete;
        wpartitionT& operator=(const wpartitionT&) = delete;

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
        bool matches(const ruleT_data& r) const {
            for (codeT code : codeT{}) {
                if (r[code] != r[head_for(code)]) {
                    return false;
                }
            }
            return true;
        }
    };

    inline vpartitionT make1() {
        vpartitionT p{};
        for (codeT code : codeT{}) {
            p[code] = code;
        }
        return p;
    }

    // TODO: concept-based partitions...
    // Every well-defined concept should be supported by a class...
    // Such a class should be able to check and generate rules...

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
        // This is important as it decides whether/how to support "s" independence...

        struct property {
            bool q, w, e, a, s, d, z, x, c;
        };

        // generative.
        vpartitionT make_partition(property req) {
            const auto [q, w, e, a, s, d, z, x, c] = req;
            const codeT mask = encode(q, w, e, a, s, d, z, x, c);
            vpartitionT p{};
            for (codeT code : codeT{}) {
                p[code] = code & mask;
            }
            return p;
        }

        // TODO: formalize requirements on interT...

        property test_property(const ruleT_data& r) {
            return {
                verify(r, {1, 0, 0, 0, 0, 0, 0, 0, 0}), //
                verify(r, {0, 1, 0, 0, 0, 0, 0, 0, 0}), //
                verify(r, {0, 0, 1, 0, 0, 0, 0, 0, 0}), //
                verify(r, {0, 0, 0, 1, 0, 0, 0, 0, 0}), //
                verify(r, {0, 0, 0, 0, 1, 0, 0, 0, 0}), //
                verify(r, {0, 0, 0, 0, 0, 1, 0, 0, 0}), //
                verify(r, {0, 0, 0, 0, 0, 0, 1, 0, 0}), //
                verify(r, {0, 0, 0, 0, 0, 0, 0, 1, 0}), //
                verify(r, {0, 0, 0, 0, 0, 0, 0, 0, 1}), //
            };
        }

        // TODO ...
        // What does this p provides?
        property test_property(const vpartitionT& p);

        // TODO: what about partial rule?

    private:
        // Does r satisfies... (TODO)
        bool verify(const ruleT_data& r, property req) {
            const auto [q, w, e, a, s, d, z, x, c] = req;
            const codeT mask = encode(q, w, e, a, s, d, z, x, c);
            for (codeT code : codeT{}) {
                if (r[code] != r[code & mask]) {
                    return false;
                }
            }
            return true;
        }
    };

    // Recognizer and... [centered] on a cell...
    struct square_symmetry {
        struct property {
            bool a; // -
            bool b; // |
            bool c; // \ (avoid ending with \)
            bool d; // /
            bool r180;
            bool r90;
        };

        vpartitionT make_partition(property req) {
            const wpartitionT* ps[]{&a_p, &b_p, &c_p, &d_p, &r180_p, &r90_p};
            auto [a, b, c, d, r180, r90] = req;
            bool reqs[]{a, b, c, d, r180, r90}; // TODO: how to do this?

            vpartitionT p = make1();
            for (int i = 0; i < 6; ++i) {
                if (reqs[i]) {
                    p = lcm(p, *ps[i], nullptr);
                }
            }
            return p;
        }

        property test_property(const ruleT_data& r) {
            return {a_p.matches(r), b_p.matches(r),    c_p.matches(r),
                    d_p.matches(r), r180_p.matches(r), r90_p.matches(r)};
        }

        property test_property(const wpartitionT& p) {
            return {a_p.subdivides(p), b_p.subdivides(p),    c_p.subdivides(p),
                    d_p.subdivides(p), r180_p.subdivides(p), r90_p.subdivides(p)};
        }

        // TODO: valid interT...
    private:
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
    };

    /*
      _/ \_
     / \_/ \
     \_/ \_/
     / \_/ \
     \_/ \_/
       \_/
    */
    // TODO: complex, has constraints on ...
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

    // TODO: should "view" and "randomize" be treated separately?
    struct state_symmetry {
        static bool test_property(const ruleT& rule) {
            for (codeT code : codeT{}) {
                codeT codex = flip_all(code);
                if ((decode_s(code) == rule(code)) != (decode_s(codex) == rule(codex))) {
                    return false;
                }
            }
            return true;
        }

        // TODO: still, how to consume?
        static inline wpartitionT p = [] {
            vpartitionT p{};
            for (codeT code : codeT{}) {
                if (decode_s(code)) {
                    p[code] = code;
                } else {
                    p[code] = flip_all(code);
                }
            }
            return p;
        }();
    };

    // Constraints consistute of:
    // partialT: should ... TO satisfy a constraint, some other traits cannot not be met...
    // neglect: the MAP rule behaves as if not ... `s` should not
    // symmetry:
    //
    // state symmetry:
    // customized: the value of... expressed by partitions...
    // TODO: how to customize?

    // -> Detect whether a rule meets certain constraints.
    // -> A way to generate all the rules that can meet certain constraints.

    // Randomization is allowed iff all constraints can be met (can co-exist)... (how?)

    // TODO: how to correctly understand interT? is it even well-defined?
    // It's for sure that, an interT poses no constraint.
} // namespace legacy

// What about uset-based partition?
#if 0
namespace legacy {
    // Problem: how to consume? how to regulate? how to join?
    class uniT {
        std::array<codeT, 512> pars;

    public:
        uniT() {
            for (codeT c : codeT{}) {
                pars[c] = c;
            }
        }

        codeT rootof(codeT c) {
            if (pars[c] == c) {
                return c;
            } else {
                return pars[c] = rootof(pars[c]);
            }
        }

        void tie(codeT a, codeT b) { pars[rootof(a)] = rootof(b); }
    };

    struct mapperT2 {
        enum takeE { _0, _1, q, w, e, a, s, d, z, x, c, nq, nw, ne, na, ns, nd, nz, nx, nc };
        takeE q2, w2, e2;
        takeE a2, s2, d2;
        takeE z2, x2, c2;

        mapperT2() : q2{q}, w2{w}, e2{e}, a2{a}, s2{s}, d2{d}, z2{z}, x2{x}, c2{c} {}

        mapperT2(std::string_view str) {
            // TODO: assert format ([01]|!?[qweasdzxc]){9}...
            const char *pos = str.data(), *end = pos + str.size();
            auto take = [&]() {
                assert(pos != end);
                bool neg = false;
                switch (*pos) {
                case '0': ++pos; return _0;
                case '1': ++pos; return _1;
                case '!':
                    ++pos;
                    neg = true;
                    break;
                }
                assert(pos != end);
                switch (*pos++) {
                case 'q': return neg ? nq : q;
                case 'w': return neg ? nw : w;
                case 'e': return neg ? ne : e;
                case 'a': return neg ? na : a;
                case 's': return neg ? ns : s;
                case 'd': return neg ? nd : d;
                case 'z': return neg ? nz : z;
                case 'x': return neg ? nx : x;
                case 'c': return neg ? nc : c;
                default: assert(false);
                }
            };
            q2 = take();
            w2 = take();
            e2 = take();
            a2 = take();
            s2 = take();
            d2 = take();
            z2 = take();
            x2 = take();
            c2 = take();
        }

        codeT map(codeT code) const {
            const envT env = decode(code);
            const bool qweasdzxc[9]{env.q, env.w, env.e, env.a, env.s, env.d, env.z, env.x, env.c};
            const auto take = [&qweasdzxc](takeE t) -> bool {
                if (t == _0) {
                    return 0;
                } else if (t == _1) {
                    return 1;
                } else if (t >= q && t <= c) {
                    return qweasdzxc[t - q];
                } else {
                    return !qweasdzxc[t - nq];
                }
            };

            // clang-format off
            return encode(take(q2), take(w2), take(e2),
                          take(a2), take(s2), take(d2),
                          take(z2), take(x2), take(c2));
            // clang-format on
        }
    };

    void tie(uniT& e, mapperT2 a, mapperT2 b = {}) {
        for (codeT c : codeT{}) {
            e.tie(a.map(c), b.map(c));
        }
    }
} // namespace legacy
#endif
