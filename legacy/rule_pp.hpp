#pragma once

// "partition.hpp" v2. Should focus more on rule concepts.

#include <functional> // TODO: use std::function? is overhead large?
#include <map>
#include <span>

#include "rule.hpp"
// TODO: this header is supposed to replace all contents in partition.hpp.
#include "partition.hpp"

namespace legacy {
    // TODO: rename / explain...
    struct partialT {
        enum stateE : char { S0, S1, Unknown };
        using data_type = std::array<stateE, 512>;
        data_type map;
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
                group_spans[j] = {
                    group_data.data() + pos[j],
                    /* TODO: ugly; without this will cause narrowing which is not allowed in {}*/ (size_t)count[j]};
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

    // TODO: concept-based partitions...
    // Every well-defined concept should be supported by a class...
    // Such a class should be able to check and generate rules...

    // TODO: what does it mean when saying a rule is independent of certain neighbors?
    // This is important as it decides whether/how to support "s" independence...

    // TODO: should "view" and "randomize" be treated separately?

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
#if 1
namespace legacy {
    // Problem: how to consume? how to regulate? how to join?
    class uniT {
        std::array<codeT, 512> parof;

    public:
        uniT() {
            for (codeT c : codeT{}) {
                parof[c] = c;
            }
        }

        // TODO: how to use publicly?
        codeT rootof(codeT c) {
            if (parof[c] == c) {
                return c;
            } else {
                return parof[c] = rootof(parof[c]);
            }
        }

        bool test(const ruleT_data& r) const {
            for (codeT code : codeT{}) {
                if (r[code] != r[parof[code]]) {
                    return false;
                }
            }
            return true;
        }

        void tie(codeT a, codeT b) { parof[rootof(a)] = rootof(b); }
    };

    // TODO: refine this "concept"...
    bool satisfies(const interT& i, const uniT& u, const ruleT& rule) {
        auto data = i.from_rule(rule);
        return u.test(data);
    }

    // A concept is
    struct conceptT {};

    struct mapperT2 {
        enum takeE { _0, _1, q, w, e, a, s, d, z, x, c, nq, nw, ne, na, ns, nd, nz, nx, nc };
        takeE q2, w2, e2;
        takeE a2, s2, d2;
        takeE z2, x2, c2;

        constexpr mapperT2(std::string_view str) {
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
                }
                assert(false);
                return _0;
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

        constexpr codeT map(codeT code) const {
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

    namespace special_mappers {
        // TODO: explain...
        inline constexpr mapperT2 identity("qweasdzxc");

        // Native symmetry.
        // Combination of these requirements can lead to ... TODO: explain, and explain "requirements"...
        inline constexpr mapperT2 asd_refl("zxc"
                                           "asd"
                                           "qwe"); // '-'
        inline constexpr mapperT2 wsx_refl("ewq"
                                           "dsa"
                                           "cxz"); // '|'
        inline constexpr mapperT2 qsc_refl("qaz"
                                           "wsx"
                                           "edc"); // '\'
        inline constexpr mapperT2 esz_refl("cde"
                                           "xsw"
                                           "zaq"); // '/'
        inline constexpr mapperT2 ro_180("cxz"
                                         "dsa"
                                         "ewq"); // 180
        inline constexpr mapperT2 ro_90("zaq"
                                        "xsw"
                                        "cde"); // 90 (clockwise)
        // TODO: does this imply 270 clockwise?

        // TODO: explain; actually irrelevant of symmetry...
        inline constexpr mapperT2 ro_45("aqw"
                                        "zse"
                                        "xcd"); // "45" clockwise. TODO: explain...
        // TODO: support that totalistic...

        // TODO: explain. TODO: better name...
        inline constexpr mapperT2 dual("!q!w!e"
                                       "!a!s!d"
                                       "!z!x!c");

        inline constexpr mapperT2 ignore_q("0weasdzxc");
        inline constexpr mapperT2 ignore_w("q0easdzxc");
        inline constexpr mapperT2 ignore_e("qw0asdzxc");
        inline constexpr mapperT2 ignore_a("qwe0sdzxc");
        inline constexpr mapperT2 ignore_s("qwea0dzxc");
        inline constexpr mapperT2 ignore_d("qweas0zxc");
        inline constexpr mapperT2 ignore_z("qweasd0xc");
        inline constexpr mapperT2 ignore_x("qweasdz0c");
        inline constexpr mapperT2 ignore_c("qweasdzx0");

        // Hexagonal emulation and emulated symmetry.

        // qw-     q w
        // asd -> a s d
        // -xc     x c
        // TODO: super problematic, especially refl...
        inline constexpr mapperT2 hex_ignore("qw0"
                                             "asd"
                                             "0xc"); // ignore_(e, z)
        // TODO: explain why 0 instead of e-z... ... is this really correct?
        inline constexpr mapperT2 hex_wsx_refl("dw0"
                                               "csq"
                                               "0xa"); // swap q-a and d-c
        inline constexpr mapperT2 hex_qsc_refl("qa0"
                                               "wsx"
                                               "0dc"); // swap ... TODO: complete...
        inline constexpr mapperT2 hex_asd_refl("xc0"
                                               "asd"
                                               "0qw"); // swap (q,x) (w,c)

        // TODO: complete...
        // TODO: better name...
        inline constexpr mapperT2 hex_qwxc_refl("wq0"
                                                "dsa"
                                                "0cx"); // swap(q,w), swap(a,d), swap(x,c)

        inline constexpr mapperT2 hex_ro_180("cx0"
                                             "dsa"
                                             "0wq"); // 180
        inline constexpr mapperT2 hex_ro_120("xa0"
                                             "csq"
                                             "0dw"); // 120 (clockwise)
        inline constexpr mapperT2 hex_ro_60("aq0"
                                            "xsw"
                                            "0cd"); // 60 (clockwise)

        // TODO: support ignore_(q, c) version?

    } // namespace special_mappers

    void tie(uniT& e, mapperT2 a, mapperT2 b = special_mappers::identity) {
        for (codeT c : codeT{}) {
            e.tie(a.map(c), b.map(c));
        }
    }

    // TODO: is this correct? can this correctly guarantee partition refinement
    bool has_tie(uniT& e, mapperT2 a, mapperT2 b = special_mappers::identity) {
        for (codeT c : codeT{}) {
            if (e.rootof(a.map(c)) != e.rootof(b.map(c))) {
                return false;
            }
        }
        return true;
    }
} // namespace legacy
#endif
