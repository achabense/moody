#pragma once

#include <map>
#include <random>
#include <span>

#include "rule.hpp"

namespace legacy {
    // TODO: Document that this is not the only situation that flicking effect can occur...
    inline bool will_flick(const ruleT& rule) {
        constexpr codeT all_0 = encode({0, 0, 0, 0, 0, 0, 0, 0, 0});
        constexpr codeT all_1 = encode({1, 1, 1, 1, 1, 1, 1, 1, 1});
        return rule(all_0) == 1 && rule(all_1) == 0;
    }

    using ruleT_masked = codeT::map_to<bool>;

    // XOR mask for ruleT.
    // TODO: operator const ruleT&()?
    struct maskT {
        ruleT viewer;

        // both methods are actually XOR...
        // TODO: better name... operator^ ?
        ruleT_masked from_rule(const ruleT& rule) const {
            ruleT_masked diff{};
            for_each_code(code) {
                diff[code] = rule(code) == viewer(code) ? 0 : 1;
            }
            return diff;
        }

        ruleT to_rule(const ruleT_masked& diff) const {
            ruleT rule{};
            for_each_code(code) {
                rule.set(code, diff[code] ? !viewer(code) : viewer(code));
            }
            return rule;
        }
    };

    inline namespace constants {
        // TODO: explain the effects of these mask...
        // rule ^ mask_zero -> TODO
        inline constexpr maskT mask_zero{{}};
        // rule ^ mask_identity -> TODO
        inline constexpr maskT mask_identity{[] {
            ruleT rule{};
            for_each_code(code) {
                rule.set(code, get_s(code));
            }
            return rule;
        }()};
    } // namespace constants

    // A mapperT defines a rule that maps each codeT to another codeT.
    // Specifically, mapperT{q2=q,w2=w,...} maps any codeT to the same value.
    struct mapperT {
        enum takeE { v0, v1, q, w, e, a, s, d, z, x, c, nq, nw, ne, na, ns, nd, nz, nx, nc };
        takeE q2, w2, e2;
        takeE a2, s2, d2;
        takeE z2, x2, c2;

        constexpr mapperT(std::string_view str) {
            // TODO: assert format ([01]|!?[qweasdzxc]){9}...
            const char *pos = str.data(), *end = pos + str.size();
            auto take2 = [&]() -> takeE {
                assert(pos != end);
                bool neg = false;
                switch (*pos) {
                case '0': ++pos; return v0;
                case '1': ++pos; return v1;
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
                default: assert(false); return v0;
                }
            };
            q2 = take2(), w2 = take2(), e2 = take2();
            a2 = take2(), s2 = take2(), d2 = take2();
            z2 = take2(), x2 = take2(), c2 = take2();
        }

        constexpr codeT operator()(codeT code) const {
            const envT env = decode(code);
            const bool qweasdzxc[9]{env.q, env.w, env.e, env.a, env.s, env.d, env.z, env.x, env.c};
            const auto take = [&qweasdzxc](takeE t) -> bool {
                if (t == v0) {
                    return 0;
                } else if (t == v1) {
                    return 1;
                } else if (t >= q && t <= c) {
                    return qweasdzxc[t - q];
                } else {
                    assert(t >= nq && t <= nc);
                    // TODO: how to silence the warning?
                    return !qweasdzxc[t - nq];
                }
            };

            // clang-format off
            return encode({take(q2), take(w2), take(e2),
                           take(a2), take(s2), take(d2),
                           take(z2), take(x2), take(c2)});
            // clang-format on
        }
    };

    // A pair of mapperT defines an equivalence relation.
    struct mapperT_pair {
        mapperT a, b;
        bool test(const ruleT_masked& rule) const {
            for_each_code(code) {
                if (rule[a(code)] != rule[b(code)]) {
                    return false;
                }
            }
            return true;
        }
    };

    // Partition of all codeT ({0}~{511}), in the form of union-find set.
    // (Lacks the ability to efficiently list groups)
    class equivT {
        mutable codeT::map_to<codeT> parof;

    public:
        equivT() {
            for_each_code(code) {
                parof[code] = code;
            }
        }

        // TODO: const or not?
        // headof?
        codeT rootof(codeT c) const {
            if (parof[c] == c) {
                return c;
            } else {
                return parof[c] = rootof(parof[c]);
            }
        }

        bool test(const ruleT_masked& r) const {
            for_each_code(code) {
                if (r[code] != r[parof[code]]) {
                    return false;
                }
            }
            return true;
        }

        void add_eq(codeT a, codeT b) { parof[rootof(a)] = rootof(b); }
        equivT& add_eq(const mapperT_pair& e) {
            for_each_code(code) {
                add_eq(e.a(code), e.b(code));
            }
            return *this;
        }
        equivT& add_eq(const equivT& e) {
            for_each_code(code) {
                // TODO: can be parof?
                add_eq(code, e.rootof(code));
            }
            return *this;
        }

        bool has_eq(codeT a, codeT b) const { return rootof(a) == rootof(b); }
        bool has_eq(const mapperT_pair& e) const {
            for_each_code(code) {
                if (!has_eq(e.a(code), e.b(code))) {
                    return false;
                }
            }
            return true;
        }
        // TODO: can this correctly check refinement-relation?
        bool has_eq(const equivT& e) const {
            for_each_code(code) {
                // TODO: can be parof?
                if (!has_eq(code, e.rootof(code))) {
                    return false;
                }
            }
            return true;
        }
    };

    inline namespace constants {
        // TODO: recheck these mappers...

        // mp_identity(any code) -> the same code
        inline constexpr mapperT mp_identity("qweasdzxc");

        // The following mappers are defined relative to mp_identity.
        // That is, the effects actually means the effects of mapperT_pair{mp_identity, mp_*}.

        // TODO: add descriptions...
        // TODO: about ignore_s and maskT...
        inline constexpr mapperT mp_ignore_q("0weasdzxc");
        inline constexpr mapperT mp_ignore_w("q0easdzxc");
        inline constexpr mapperT mp_ignore_e("qw0asdzxc");
        inline constexpr mapperT mp_ignore_a("qwe0sdzxc");
        inline constexpr mapperT mp_ignore_s("qwea0dzxc");
        inline constexpr mapperT mp_ignore_d("qweas0zxc");
        inline constexpr mapperT mp_ignore_z("qweasd0xc");
        inline constexpr mapperT mp_ignore_x("qweasdz0c");
        inline constexpr mapperT mp_ignore_c("qweasdzx0");

        // TODO: clarify the [exact] meaning (&&effects) of these mappers...
        // Native symmetry.
        // Combination of these requirements can lead to ... TODO: explain, and explain "requirements"...
        inline constexpr mapperT mp_refl_asd("zxc"
                                             "asd"
                                             "qwe"); // '-'
        inline constexpr mapperT mp_refl_wsx("ewq"
                                             "dsa"
                                             "cxz"); // '|'
        inline constexpr mapperT mp_refl_qsc("qaz"
                                             "wsx"
                                             "edc"); // '\'
        inline constexpr mapperT mp_refl_esz("cde"
                                             "xsw"
                                             "zaq"); // '/'
        inline constexpr mapperT mp_C2("cxz"
                                       "dsa"
                                       "ewq"); // 180
        inline constexpr mapperT mp_C4("zaq"
                                       "xsw"
                                       "cde"); // 90 (clockwise)
        // 90 has the same effects as 270... TODO: recheck...

        // TODO: explain; actually irrelevant of symmetry...
        // 1. I misunderstood "rotate" symmetry. "ro45" is never about symmetry (I've no idea what it is)
        // 2. As seemingly-senseless partition like ro45 can make non-trivial patterns, should support after all...
        // TODO: explain C8 (not related to static symmetry, but covers C4 and works with tot_xxc_s)
        inline constexpr mapperT mp_C8("aqw"
                                       "zse"
                                       "xcd");
        inline constexpr mapperT mp_tot_exc_s("wqe"
                                              "asd"
                                              "zxc"); // *C8 -> totalistic, excluding s
        inline constexpr mapperT mp_tot_inc_s("qse"
                                              "awd"
                                              "zxc"); // *C8 -> totalistic, including s

        // TODO: explain. TODO: better name...
        inline constexpr mapperT mp_dual("!q!w!e"
                                         "!a!s!d"
                                         "!z!x!c");

        // Hexagonal emulation and emulated symmetry.

        // qw-     q w
        // asd -> a s d
        // -xc     x c
        // TODO: explain why 0 instead of e-z... ... is this really correct?
        inline constexpr mapperT mp_hex_ignore("qw0"
                                               "asd"
                                               "0xc"); // ignore_(e, z)

        inline constexpr mapperT mp_hex_refl_asd("xc0"
                                                 "asd"
                                                 "0qw"); // swap(q,x), swap(w,c)
        inline constexpr mapperT mp_hex_refl_qsc("qa0"
                                                 "wsx"
                                                 "0dc"); // swap(a,w), swap(x,d)
        inline constexpr mapperT mp_hex_refl_wsx("dw0"
                                                 "csq"
                                                 "0xa"); // swap(q,d), swap(a,c)

        inline constexpr mapperT mp_hex_refl_aq("ax0"
                                                "qsc"
                                                "0wd"); // swap(a,q), swap(x,w), swap(c,d)
        inline constexpr mapperT mp_hex_refl_qw("wq0"
                                                "dsa"
                                                "0cx"); // swap(q,w), swap(a,d), swap(x,c)
        inline constexpr mapperT mp_hex_refl_wd("cd0"
                                                "xsw"
                                                "0aq"); // swap(w,d), swap(q,c), swap(a,x)

        inline constexpr mapperT mp_hex_C2("cx0"
                                           "dsa"
                                           "0wq"); // 180
        inline constexpr mapperT mp_hex_C3("xa0"
                                           "csq"
                                           "0dw"); // 120 (clockwise)
        inline constexpr mapperT mp_hex_C6("aq0"
                                           "xsw"
                                           "0cd"); // 60 (clockwise)

        inline constexpr mapperT mp_hex_tot_exc_s("wq0"
                                                  "asd"
                                                  "0xc"); // *C6 -> totalistic, excluding s
        inline constexpr mapperT mp_hex_tot_inc_s("qs0"
                                                  "awd"
                                                  "0xc"); // *C6 -> totalistic, including s

        // -we     w e
        // asd -> a s d
        // zx-     z x
        inline constexpr mapperT mp_hex2_ignore("0we"
                                                "asd"
                                                "zx0"); // ignore_(q, c)

    } // namespace constants

    using groupT = std::span<const codeT>;

    // With full capacity...
    class partitionT {
        using indexT = int;
        codeT::map_to<indexT> m_p;
        int m_k;

        // Though with size 512, m_data is not a map for codeT.
        // Instead, it stores codeT of the same group consecutively.
        // (In short, m_data[codeT] has no meaning related to codeT itself...)
        std::array<codeT, 512> m_data;
        struct group_pos {
            int pos, size;
        };
        // TODO: there is std::sample; will be very convenient if storing directly...
        // Pro: directly-copyable.
        // Con: harder to use...
        std::vector<group_pos> m_groups;

    public:
        int k() const { return m_k; }
        groupT jth_group(indexT j) const {
            const auto [pos, size] = m_groups[j];
            return groupT(m_data.data() + pos, size);
        }

        // TODO: whether to expose index?
        indexT index_for(codeT code) const { return m_p[code]; }
        groupT group_for(codeT code) const { return jth_group(index_for(code)); }
        // TODO: when is head for needed?
        // TODO: why not directly store a equivT?
        codeT head_for(codeT code) const {
            // group_for(code).front();
            return m_data[m_groups[index_for(code)].pos];
        }

        explicit partitionT(const equivT& u) {
            std::map<codeT, indexT> m;
            indexT i = 0;
            for_each_code(code) {
                codeT head = u.rootof(code);
                if (!m.contains(head)) {
                    m[head] = i++;
                }
                m_p[code] = m[head];
            }

            m_k = i;

            std::vector<int> count(m_k, 0);
            for (indexT col : m_p) {
                ++count[col];
            }

            std::vector<int> pos(m_k, 0);
            for (int j = 1; j < m_k; ++j) {
                pos[j] = pos[j - 1] + count[j - 1];
            }

            m_groups.resize(m_k);
            for (int j = 0; j < m_k; ++j) {
                m_groups[j] = {pos[j], count[j]};
            }

            for_each_code(code) {
                indexT col = m_p[code];
                m_data[pos[col]++] = code;
            }
        }
        // TODO: Is this needed?
        bool test(const ruleT_masked& r) const {
            for_each_code(code) {
                if (r[code] != r[head_for(code)]) {
                    return false;
                }
            }
            return true;
        }
        // TODO: is refinement checking needed for partitionT?
        // TODO: refer to https://en.wikipedia.org/wiki/Partition_of_a_set#Refinement_of_partitions
    };

    // TODO: refine "concept"...
    // 1. Does a rule satisfy the concept.
    // 2. How to generate all possible rules that satisfy some concepts...
    inline bool satisfies(const ruleT& rule, const maskT& mask, const mapperT_pair& q) {
        return q.test(mask.from_rule(rule));
    }
    inline bool satisfies(const ruleT& rule, const maskT& mask, const equivT& q) {
        return q.test(mask.from_rule(rule));
    }
    inline bool satisfies(const ruleT& rule, const maskT& mask, const partitionT& q) {
        return q.test(mask.from_rule(rule));
    }

    inline void flip(groupT group, ruleT& rule) {
        for (codeT c : group) {
            rule.set(c, !rule(c));
        }
    }

    // TODO: obscure???
    inline void copy(groupT group, const ruleT& source, ruleT& dest) {
        for (codeT c : group) {
            dest.set(c, source(c));
        }
    }

    using lockT = codeT::map_to<bool>;

    inline auto scan(const partitionT& par, const ruleT_masked& rule, const lockT& locked) {
        struct counterT {
            int free_0 = 0, free_1 = 0;
            int locked_0 = 0, locked_1 = 0;

            bool any_locked() const { return locked_0 || locked_1; }
            bool none_locked() const { return !any_locked(); }
            bool all_locked() const { return !free_0 && !free_1; }

            bool all_0() const { return !free_1 && !locked_1; }
            bool all_1() const { return !free_0 && !locked_0; }
            bool inconsistent() const { return !all_0() && !all_1(); }
        };

        std::vector<counterT> vec(par.k());
        for (int j = 0; j < par.k(); ++j) {
            for (codeT code : par.jth_group(j)) {
                if (locked[code]) {
                    rule[code] ? ++vec[j].locked_1 : ++vec[j].locked_0;
                } else {
                    rule[code] ? ++vec[j].free_1 : ++vec[j].free_0;
                }
            }
        }
        return vec;
    }

    inline bool any_locked(const lockT& locked, const groupT group) {
        return std::any_of(group.begin(), group.end(), [&locked](codeT code) { return locked[code]; });
    }

    inline bool all_locked(const lockT& locked, const groupT group) {
        return std::all_of(group.begin(), group.end(), [&locked](codeT code) { return locked[code]; });
    }

    // TODO: blindly applying enhance can lead to more inconsistent locked groups...
    inline void enhance(const partitionT& par, lockT& locked) {
        for (int j = 0; j < par.k(); ++j) {
            const groupT group = par.jth_group(j);
            if (any_locked(locked, group)) {
                for (const codeT code : group) {
                    locked[code] = true;
                }
            }
        }
    }

    // TODO: explain...
    inline ruleT purify(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
        // TODO: for locked code A and B, what if r[A] != r[B]?
        ruleT_masked r = mask.from_rule(rule);
        for (int j = 0; j < par.k(); ++j) {
            const groupT group = par.jth_group(j);
            // TODO: should be scan-based... what if inconsistent?
            const auto fnd = std::find_if(group.begin(), group.end(), [&locked](codeT code) { return locked[code]; });
            if (fnd != group.end()) {
                const bool b = r[*fnd];
                for (codeT code : group) {
                    r[code] = b;
                }
            }
        }
        return mask.to_rule(r);
    }

    // TODO: replace with stricter version...
    inline ruleT purify_v2(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked);

    // TODO: the lockT will become meaningless on irrelevant rule switch (clipboard/file...)

    // TODO: should (lr/up/)mirror conversions modify locks as well?

    inline int count_free(const partitionT& par, const lockT& locked) {
        int free = 0;
        for (int j = 0; j < par.k(); ++j) {
            if (!any_locked(locked, par.jth_group(j))) {
                ++free;
            }
        }
        return free;
    }

    // TODO: what's the best way to pass fn?
    // TODO: is `redispatch` a suitable name?
    // TODO: whether to skip/allow inconsistent groups?
    inline ruleT redispatch(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked,
                            auto fn /*void(bool* begin, bool* end)*/) {
        // TODO: precondition?

        ruleT_masked r = mask.from_rule(rule);

        std::vector<int> free_indexes;
        for (int j = 0; j < par.k(); ++j) {
            if (!any_locked(locked, par.jth_group(j))) {
                free_indexes.push_back(j);
            }
        }

        // TODO: explain bools is not a map for ruleT...
        // TODO: use other containers instead?
        // TODO: are there better ways than using front?
        std::array<bool, 512> bools{};
        for (int j = 0; j < free_indexes.size(); ++j) {
            bools[j] = r[par.jth_group(free_indexes[j]).front()];
        }

        fn(bools.data(), bools.data() + free_indexes.size());

        for (int j = 0; j < free_indexes.size(); ++j) {
            // TODO: cannot suitably deal with inconsistent groups...
            for (codeT code : par.jth_group(free_indexes[j])) {
                r[code] = bools[j];
            }
        }
        return mask.to_rule(r);
    }

    // TODO: count_min denotes free groups now; whether to switch to total groups (at least in the gui part)?
    inline ruleT randomize(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked,
                           std::mt19937& rand, int count_min,
                           int count_max /* not used, subject to future extension */) {
        assert(count_max == count_min);
        return redispatch(mask, par, rule, locked, [&rand, count_min](bool* begin, bool* end) {
            int c = std::clamp(count_min, 0, int(end - begin));
            std::fill(begin, end, 0);
            std::fill_n(begin, c, 1);
            std::shuffle(begin, end, rand);
        });
    }

    inline ruleT shuffle(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked,
                         std::mt19937& rand) {
        return redispatch(mask, par, rule, locked, [&rand](bool* begin, bool* end) { std::shuffle(begin, end, rand); });
    }

    // TODO: rename to [set_]first / ...
    struct act_int {
        // TODO: disable !par.test(...) checks...
        static ruleT first(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) { std::fill(begin, end, 0); });
        }

        static ruleT last(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) { std::fill(begin, end, 1); });
        }

        static ruleT next(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) {
                // 11...0 -> 00...1; stop at 111...1 (last())
                bool* first_0 = std::find(begin, end, 0);
                if (first_0 != end) {
                    std::fill(begin, first_0, 0);
                    *first_0 = 1;
                }
            });
        }

        static ruleT prev(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) {
                // 00...1 -> 11...0; stop at 000...0 (first())
                bool* first_1 = std::find(begin, end, 1);
                if (first_1 != end) {
                    std::fill(begin, first_1, 1);
                    *first_1 = 0;
                }
            });
        }
    };

    // TODO: ++/--count when reaching end?
    // Intentionally using reverse_iterator... TODO: explain why...
    // (TODO: rephrase) As to CTAD vs make_XXX..., here is pitfall for using std::reverse_iterator directly.
    // https://quuxplusone.github.io/blog/2022/08/02/reverse-iterator-ctad/
    struct act_perm {
        static ruleT first(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) {
                int c = std::count(begin, end, 1);
                std::fill(begin, end, 0);
                std::fill_n(begin, c, 1);
            });
        }

        static ruleT last(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) {
                int c = std::count(begin, end, 1);
                std::fill(begin, end, 0);
                std::fill_n(end - c, c, 1);
            });
        }

        static ruleT next(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) {
                // Stop at 000...111 (last())
                if (std::find(std::find(begin, end, 1), end, 0) == end) {
                    return;
                }

                std::next_permutation(std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
            });
        }

        static ruleT prev(const maskT& mask, const partitionT& par, const ruleT& rule, const lockT& locked) {
            return redispatch(mask, par, rule, locked, [](bool* begin, bool* end) {
                // Stop at 111...000 (first())
                if (std::find(std::find(begin, end, 0), end, 1) == end) {
                    return;
                }

                std::prev_permutation(std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
            });
        }
    };

} // namespace legacy

// TODO: define xxxT = {rule, locked}?
namespace legacy {
    // TODO: other actions... (lr/ud/diag/counter-diag/...)

    // TODO: proper name...
    inline ruleT mirror(const ruleT& rule) {
        ruleT mir{};
        for_each_code(code) {
            const codeT codex = codeT(~code & 511);
            const bool flip = get_s(codex) != rule(codex);
            mir.set(code, flip ? !get_s(code) : get_s(code));
        }
        return mir;
    }

    inline bool satisfies(const ruleT& rule, const lockT& locked, const maskT& mask, const equivT& e) {
        const ruleT_masked r = mask.from_rule(rule);
        codeT::map_to<int> record;
        record.fill(2);
        for_each_code(code) {
            if (locked[code]) {
                int& rep = record[e.rootof(code)];
                if (rep == 2) {
                    rep = r[code];
                } else if (rep != r[code]) {
                    return false;
                }
            }
        }
        return true;
    }
} // namespace legacy

#include <optional> // TODO: in this header optional is used to represent empty set. not very suitable...

namespace legacy {
    // TODO: it seems that, if the underlying mask is essentially different (not interchangeable), the
    // result of & of two subsetT can result in | of multiple subsetT...
    struct subsetT {
        maskT mask;
        equivT eq;

        bool contains(const ruleT& rule) const { return eq.test(mask.from_rule(rule)); }
        bool contains(const subsetT& other) const { return contains(other.mask.viewer) && other.eq.has_eq(eq); }
        bool equals(const subsetT& other) const { return contains(other) && other.contains(*this); }
        bool change_mask(const maskT& mask2) {
            if (!contains(mask2.viewer)) {
                return false;
            }

            mask = mask2;
            return true;
        }

        // TODO...
        ruleT approximate(const ruleT& r) const;

        // Temporary...
        ruleT random_rule(int count, std::mt19937& rand) const {
            partitionT par(eq); // TODO: expensive...
            enum boolT : bool {};
            std::vector<boolT> ss(par.k());
            for (int i = 0; i < ss.size() && i < count; ++i) {
                ss[i] = boolT{true};
            }
            std::shuffle(ss.begin(), ss.end(), rand);
            ruleT rule = mask.viewer;
            for (int j = 0; j < ss.size(); ++j) {
                if (ss[j]) { // TODO: how is this allowed in the standard?
                    for (codeT code : par.jth_group(j)) {
                        rule.set(code, !rule(code));
                    }
                }
            }
            return rule;
        }

        // prove that for arbitrary subsetT a and b, a & b result in either an empty set, or still another subsetT:
        // 1. prove a & b may result in an empty set ({}).
        // 2. suppose a & b != {}, so there is a rule r ∈ a and ∈ b:
        // look at any [codeT] of it.
        // Now flip [codeT] as well as the rest [codeT] in the [groupT]. It's obvious that, such a rule still belongs to
        // both a and b.
        // 3. notice that determines the rest [codeT]s in the group, so there is no outside of {{r},...}...
        // ... no, not strictly uniquely deterministic...

        // nullopt: empty set...
        friend std::optional<subsetT> operator&(const std::optional<subsetT>& a_op,
                                                const std::optional<subsetT>& b_op) {
            if (!a_op || !b_op) {
                return std::nullopt;
            }
            const subsetT &a = *a_op, &b = *b_op;

            equivT eq_both = equivT(a.eq).add_eq(b.eq);
            partitionT par_a(a.eq), par_b(b.eq), par_both(eq_both);

            // find a rule that is contained by both a and b...
            ruleT common_rule{};
            codeT::map_to<bool> done{};

// TODO: it turns out that, the result is not deterministic - can be dependent on certain invocation sequence...
// So, & may not result in a single subsetT (but | of multiple subsetTs)...
#if 1
            std::array<codeT, 512> codes;
            for_each_code(code) {
                auto [q, w, e, a, s, d, z, x, c] = decode(code);
                codes[q * 256 + w * 2 + e * 4 + a * 8 + s * 16 + d * 32 + z * 64 + x * 128 + c * 1] = code;
            }

            for (codeT code : codes) {
#else
            for_each_code(code) {
#endif
                if (!done[code]) {
                    for (codeT c : par_both.group_for(code)) {
                        assert(!done[c]);
                    }

                    auto transfer_dependency = [&a, &b, &done, &common_rule, &par_a, &par_b](codeT code, bool v,
                                                                                             auto& self) -> bool {
                        if (done[code]) {
                            if (common_rule(code) != v) {
                                return false;
                            }
                        } else {
                            done[code] = true;
                            common_rule.set(code, v);
                            {
                                const bool viewed_by_a = a.mask.viewer(code) ^ v;
                                for (codeT c : par_a.group_for(code)) {
                                    if (!self(c, a.mask.viewer(c) ^ viewed_by_a, self)) {
                                        return false;
                                    }
                                }
                            }
                            {
                                bool viewed_by_b = b.mask.viewer(code) ^ v;
                                for (codeT c : par_b.group_for(code)) {
                                    if (!self(c, b.mask.viewer(c) ^ viewed_by_b, self)) {
                                        return false;
                                    }
                                }
                            }
                        }
                        return true;
                    };

                    // TODO: 0 or a.mask.viewer(code)?
                    if (!transfer_dependency(code, a.mask.viewer(code), transfer_dependency)) {
                        return std::nullopt;
                    }

                    for (codeT c : par_both.group_for(code)) {
                        assert(done[c]);
                    }
                }
            }
            assert(a.contains(subsetT{.mask{common_rule}, .eq{eq_both}}));
            assert(b.contains(subsetT{.mask{common_rule}, .eq{eq_both}}));
            return subsetT{.mask{common_rule}, .eq{eq_both}};
        }
    };

    inline static const subsetT test_ignore_s_and_self_cmpl = [] {
        if (1) {
            subsetT sa{mask_zero, equivT{}.add_eq({mp_ignore_s, mp_identity})};
            subsetT sb{mask_identity, equivT{}.add_eq({mp_dual, mp_identity})};

            auto sc = sa & sb;
            assert(sc);
            return *sc;
        } else {
            subsetT s0{mask_zero, equivT{}.add_eq({mp_refl_qsc, mp_identity})};
            subsetT s1{mask_zero, equivT{}.add_eq({mp_C4, mp_identity})};
            std::mt19937 rand;
            s0.change_mask({s0.random_rule(10, rand)});
            s1.change_mask({s1.random_rule(12, rand)});

            auto s2 = s0 & s1;
            assert(s2);
            return *s2;
        }
    }();
} //  namespace legacy
