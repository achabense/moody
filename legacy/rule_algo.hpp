#pragma once

#include "rule.hpp"

namespace legacy {
    // A maskT is a ruleT used to do XOR mask for other rules.
    // The result reflects how the rule is different from the masking rule.
    struct maskT : public ruleT {};
    using ruleT_masked = codeT::map_to<bool>;

    inline void operator^(const maskT&, const maskT&) = delete;

    inline ruleT_masked operator^(const maskT& mask, const ruleT& rule) {
        ruleT_masked r{};
        for_each_code([&](codeT code) { r[code] = mask[code] ^ rule[code]; });
        return r;
    }

    inline ruleT operator^(const maskT& mask, const ruleT_masked& r) {
        ruleT rule{};
        for_each_code([&](codeT code) { rule[code] = mask[code] ^ r[code]; });
        return rule;
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_maskT = [] {
            const ruleT a = make_rule([](auto) { return testT::rand() & 1; });
            const ruleT b = make_rule([](auto) { return testT::rand() & 1; });
            const maskT a_as_mask{a}, b_as_mask{b};

            assert(a == (b_as_mask ^ (b_as_mask ^ a)));
            assert(b == (a_as_mask ^ (a_as_mask ^ b)));
        };
    }
#endif // ENABLE_TESTS

    // Equivalence relation for codeT ({0...511}), in the form of union-find set.
    class equivT {
        friend class partitionT;

        mutable codeT::map_to<codeT> parof;

        codeT headof(codeT c) const {
            if (parof[c] == c) {
                return c;
            } else {
                return parof[c] = headof(parof[c]);
            }
        }

    public:
        equivT() {
            for_each_code([&](codeT code) { parof[code] = code; });
        }

        // Does `r` have the same value in each group?
        bool test(const ruleT_masked& r) const {
            return for_each_code_all_of([&](codeT code) { //
                return r[code] == r[parof[code]];
            });
        }

        // Does `r` have the same value for locked codes in each group?
        bool test(const ruleT_masked& r, const moldT::lockT& lock) const {
            codeT::map_to<int> record;
            record.fill(-1);

            return for_each_code_all_of([&](codeT code) {
                if (!lock[code]) {
                    return true;
                }

                int& rep = record[headof(code)];
                if (rep == -1) {
                    rep = r[code];
                }
                return rep == r[code];
            });
        }

        void add_eq(codeT a, codeT b) { parof[headof(a)] = headof(b); }
        void add_eq(const equivT& other) {
            for_each_code([&](codeT code) { add_eq(code, other.parof[code]); });
        }

        bool has_eq(codeT a, codeT b) const { return headof(a) == headof(b); }
        bool has_eq(const equivT& other) const {
            return for_each_code_all_of([&](codeT code) { //
                return has_eq(code, other.parof[code]);
            });
        }

        friend equivT operator|(const equivT& a, const equivT& b) {
            equivT c = a;
            c.add_eq(b);
            return c;
        }
    };

    using groupT = std::span<const codeT>;

    class partitionT {
        equivT m_eq;
        int m_k{}; // `m_eq` has `m_k` groups.

        // Map codeT to an integer ∈ [0, m_k) (which represents the group the code belongs to).
        codeT::map_to<int> m_map{};

        // (Not a codeT::map_to<codeT>)
        // Store codeT of the same group consecutively (to provide codeT span).
        std::array<codeT, 512> m_data{};

        struct group_pos {
            int pos, size;
        };
        std::vector<group_pos> m_groups{};

        groupT jth_group(int j) const {
            const auto [pos, size] = m_groups[j];
            return groupT(m_data.data() + pos, size);
        }

    public:
        // ~ "user-declared" to avoid implicit moving (to avoid `m_data` being emptied):
        partitionT(const partitionT&) = default;
        partitionT& operator=(const partitionT&) = default;

        groupT group_for(codeT code) const { return jth_group(m_map[code]); }

        int k() const { return m_k; }
        void for_each_group(const auto& fn) const {
            for (int j = 0; j < m_k; ++j) {
                if constexpr (requires { fn(jth_group(j)); }) {
                    fn(jth_group(j));
                } else {
                    fn(j, jth_group(j));
                }
            }
        }

        /*implicit*/ partitionT(const equivT& u) : m_eq(u) {
            m_k = 0;
            m_map.fill(-1);
            for_each_code([&](codeT code) {
                const codeT head = u.headof(code);
                if (m_map[head] == -1) {
                    m_map[head] = m_k++;
                }
                m_map[code] = m_map[head];
            });
            // m_k is now the number of groups in the partition.

            std::vector<int> count(m_k, 0);
            for_each_code([&](codeT code) { ++count[m_map[code]]; });

            std::vector<int> pos(m_k, 0);
            for (int j = 1; j < m_k; ++j) {
                pos[j] = pos[j - 1] + count[j - 1];
            }

            m_groups.resize(m_k);
            for (int j = 0; j < m_k; ++j) {
                m_groups[j] = {pos[j], count[j]};
            }

            for_each_code([&](codeT code) {
                int j = m_map[code];
                m_data[pos[j]++] = code;
            });
        }

        bool test(const ruleT_masked& r) const { return m_eq.test(r); }
        bool test(const ruleT_masked& r, const moldT::lockT& lock) const { return m_eq.test(r, lock); }

        bool is_refinement_of(const partitionT& other) const { return other.m_eq.has_eq(m_eq); }

        friend partitionT operator|(const partitionT& a, const partitionT& b) { //
            return partitionT(a.m_eq | b.m_eq);
        }
    };

    // TODO ...
    // A subsetT (s = {} or (m, p)) defines a subset of all possible ruleT, where:
    // ...
    // (As a result, any rule in the subset can be used as the mask without ...)
    // About interaction with `moldT`...
    class subsetT {
        struct nonemptyT {
            maskT mask;
            partitionT par;

            bool contains(const ruleT& rule) const { return par.test(mask ^ rule); }
            bool includes(const nonemptyT& other) const {
                return contains(other.mask) && par.is_refinement_of(other.par);
            }
        };

        std::optional<nonemptyT> m_set;

    public:
        explicit subsetT(const maskT& mask, const equivT& eq) { m_set.emplace(mask, eq); }
        explicit subsetT(const maskT& mask, const partitionT& par) { m_set.emplace(mask, par); }

        explicit subsetT() : m_set{std::nullopt} {}
        static subsetT universal() { return subsetT{maskT{}, equivT{}}; }

        bool empty() const { return !m_set.has_value(); }
        bool contains(const ruleT& rule) const { return m_set && m_set->contains(rule); }
        bool includes(const subsetT& other) const { return other.empty() || (m_set && m_set->includes(*other.m_set)); }
        bool equals(const subsetT& other) const { return includes(other) && other.includes(*this); }

        const maskT& get_mask() const {
            assert(!empty());
            return m_set->mask;
        }
        const partitionT& get_par() const {
            assert(!empty());
            return m_set->par;
        }

        // Look for a rule that both a and b contains.
        static std::optional<maskT> common_rule(const subsetT& a_, const subsetT& b_) {
            if (a_.empty() || b_.empty()) {
                return std::nullopt;
            }

            const nonemptyT &a = *a_.m_set, &b = *b_.m_set;

            if (a.contains(b.mask)) {
                return b.mask;
            } else if (b.contains(a.mask)) {
                return a.mask;
            }

            maskT common{};
            codeT::map_to<bool> assigned{};

            // TODO: this is really slow in debug mode... the complexity seems 512^2 in worst case.
            // TODO: explain try-assign will result a correct rule iff a & b != {}.
            auto try_assign = [&](const codeT code, const bool v, auto& self) -> void {
                if (!assigned[code]) {
                    assigned[code] = true;
                    common[code] = v;
                    const bool masked_by_a = a.mask[code] ^ v;
                    const bool masked_by_b = b.mask[code] ^ v;
                    for (const codeT c : a.par.group_for(code)) {
                        self(c, a.mask[c] ^ masked_by_a, self);
                    }
                    for (const codeT c : b.par.group_for(code)) {
                        self(c, b.mask[c] ^ masked_by_b, self);
                    }
                }
            };

#ifndef NDEBUG
            const partitionT par_both = a.par | b.par;
#endif
            for_each_code([&](codeT code) {
                if (!assigned[code]) {
                    assert(std::ranges::none_of(par_both.group_for(code), [&](codeT c) { return assigned[c]; }));
                    try_assign(code, 0, try_assign);
                    assert(std::ranges::all_of(par_both.group_for(code), [&](codeT c) { return assigned[c]; }));
                }
            });

            if (!a.contains(common) || !b.contains(common)) {
                return std::nullopt;
            }
            return common;
        }
    };

    // The intersection (&) of any two subsetT (a) and (b) is still another subsetT:
    // If there is no common rule, then (a & b) results in an empty set.
    // Otherwise, suppose there is a rule (r) known to belong to (a & b):
    // Let (p) = (a.par | b.par), and (s) = subsetT(r, p), then:
    // 1. (s) is included by (a & b), as:
    // Any rule in (s) can be gotten by flipping some groups in (p) from (r), and as (a.par) and (b.par)
    // are refinements of (p), the rule can also be viewed as flipping some groups in (a.par) or (b.par),
    // so the rule also belongs to (a) and (b).
    // 2. There cannot be a rule that belongs to (a & b) but not (s) (so (a & b) is included by (s)), as:
    // The values of a rule in any group from (p) are inter-dependent. If the value for any codeT (c) is fixed,
    // then the values for the group (c) belongs in (p) are also fixed accordingly.
    // As a result, the rule must be able to be flipped by whole groups from (r), so belongs to (s)
    inline subsetT operator&(const subsetT& a, const subsetT& b) {
        if (auto common = subsetT::common_rule(a, b)) {
            assert(!a.empty() && !b.empty());
            return subsetT{*common, a.get_par() | b.get_par()};
        }

        return subsetT{};
    }

    // TODO: enhance...
    inline auto scan(const partitionT& par, const ruleT_masked& rule, const moldT::lockT& lock) {
        struct counterT {
            int free_0 = 0, free_1 = 0;
            int locked_0 = 0, locked_1 = 0; // 0/1 means masked value.

            bool any_locked() const { return locked_0 || locked_1; }
            bool none_locked() const { return !any_locked(); }
            bool all_locked() const { return !free_0 && !free_1; }

            bool all_0() const { return !free_1 && !locked_1; }
            bool all_1() const { return !free_0 && !locked_0; }
            bool inconsistent() const { return !all_0() && !all_1(); }
        };

        std::vector<counterT> vec(par.k());
        par.for_each_group([&](int j, const groupT& group) {
            for (codeT code : group) {
                if (lock[code]) {
                    rule[code] ? ++vec[j].locked_1 : ++vec[j].locked_0;
                } else {
                    rule[code] ? ++vec[j].free_1 : ++vec[j].free_0;
                }
            }
        });
        return vec;
    }

    inline bool any_locked(const moldT::lockT& lock, const groupT& group) {
        return std::ranges::any_of(group, [&lock](codeT code) { return lock[code]; });
    }

    inline bool all_locked(const moldT::lockT& lock, const groupT& group) {
        return std::ranges::all_of(group, [&lock](codeT code) { return lock[code]; });
    }

    inline bool none_locked(const moldT::lockT& lock, const groupT& group) {
        return std::ranges::none_of(group, [&lock](codeT code) { return lock[code]; });
    }

    // TODO: used in `edit_rule`; avoidable if scan early...
    inline int count_free(const partitionT& par, const moldT::lockT& lock) {
        int free = 0;
        par.for_each_group([&](const groupT& group) {
            if (none_locked(lock, group)) {
                ++free;
            }
        });
        return free;
    }

    // TODO: explain why requiring subset.contains(mold.rule).
    inline moldT::lockT enhance_lock(const subsetT& subset, const moldT& mold) {
        assert(subset.contains(mold.rule));

        moldT::lockT lock{};
        subset.get_par().for_each_group([&](const groupT& group) {
            if (any_locked(mold.lock, group)) {
                for (codeT code : group) {
                    lock[code] = true;
                }
            }
        });
        return lock;
    }

    // TODO: (temp) experimental... may consider the "forging mode" approach finally.
    // Notice this is different from flipping the values of each lock[code].
    inline moldT::lockT invert_lock(const subsetT& subset, const moldT& mold) {
        assert(subset.contains(mold.rule));

        moldT::lockT lock{};
        subset.get_par().for_each_group([&](const groupT& group) {
            if (none_locked(mold.lock, group)) {
                for (codeT code : group) {
                    lock[code] = true;
                }
            }
        });
        return lock;
    }

    // Does there exist any rule that belongs to both `subset` and `mold`?
    // TODO: about moldT::compatible...
    inline bool compatible(const subsetT& subset, const moldT& mold) {
        if (subset.empty()) {
            return false;
        }

        const ruleT_masked r = subset.get_mask() ^ mold.rule;
        return subset.get_par().test(r, mold.lock);
    }

    // Return a rule that belongs to `subset` and `mold` and is closest to `mold.rule`.
    // (If `mold.rule` already belongs to `subset`, the result should be exactly `mold.rule`.)
    inline ruleT approximate(const subsetT& subset, const moldT& mold) {
        assert(compatible(subset, mold));

        const maskT& mask = subset.get_mask();
        const partitionT& par = subset.get_par();
        const auto scanlist = scan(par, mask ^ mold.rule, mold.lock);

        ruleT_masked r{};
        par.for_each_group([&](int j, const groupT& group) {
            const auto& scan = scanlist[j];
            // TODO: explain; recheck...
            const bool v = scan.locked_0 ? 0 : scan.locked_1 ? 1 : scan.free_0 > scan.free_1 ? 0 : 1;
            for (codeT code : group) {
                r[code] = v;
            }
        });

        const ruleT res = mask ^ r;
        assert(subset.contains(res) && mold.compatible(res));
        // subset.contains(mold.rule) -> mold.rule == res:
        assert(!subset.contains(mold.rule) || (mold.rule == res));
        return res;
    }

    // TODO: explain...
    // X Also, it might be helpful to support "in-lock" forging. For example, to dial to find potentially related
    // X patterns...
    // X Directly invert the locks, or add a flag in `forge`? (TODO: recheck `invert_lock`)
    inline ruleT transform(const subsetT& subset, const maskT& mask, const moldT& mold,
                           std::invocable<bool*, bool*> auto fn) {
        assert(subset.contains(mask)); // TODO ... about why not using subset.get_mask()...
        assert(compatible(subset, mold));

        const partitionT& par = subset.get_par();

        ruleT_masked r = mask ^ approximate(subset, mold);

        // `seq` is not a codeT::map_to<bool>.
        assert(par.k() <= 512);
        std::array<bool, 512> seq{};
        int z = 0;
        par.for_each_group([&](const groupT& group) {
            if (none_locked(mold.lock, group)) {
                seq[z] = r[group[0]];
                ++z;
            }
        });

        fn(seq.data(), seq.data() + z);

        z = 0;
        par.for_each_group([&](const groupT& group) {
            if (none_locked(mold.lock, group)) {
                for (codeT code : group) {
                    r[code] = seq[z];
                }
                ++z;
            }
        });

        const ruleT res = mask ^ r;
        assert(subset.contains(res) && mold.compatible(res));
        return res;
    }

    // TODO: `count` denotes free groups now; whether to switch to total groups (at least in the gui part)?
    // TODO: (temp) there was a plan to support count_min~count_max mode finally... dropped now.
    inline ruleT randomize(const subsetT& subset, const maskT& mask, const moldT& mold, std::mt19937& rand, int count) {
        assert(compatible(subset, mold));

        return transform(subset, mask, mold, [&rand, count](bool* begin, bool* end) {
            int c = std::clamp(count, 0, int(end - begin));
            std::fill(begin, end, 0);
            std::fill_n(begin, c, 1);
            std::shuffle(begin, end, rand);
        });
    }

    inline ruleT randomize_v2(const subsetT& subset, const maskT& mask, const moldT& mold, std::mt19937& rand,
                              double density) {
        assert(compatible(subset, mold));

        return transform(subset, mask, mold, [&rand, density](bool* begin, bool* end) {
            std::bernoulli_distribution dist(std::clamp(density, 0.0, 1.0));
            std::generate(begin, end, [&] { return dist(rand); });
        });
    }

    inline ruleT shuffle(const subsetT& subset, const maskT& mask, const moldT& mold, std::mt19937& rand) {
        assert(subset.contains(mold.rule));

        return transform(subset, mask, mold, [&rand](bool* begin, bool* end) { std::shuffle(begin, end, rand); });
    }

    // TODO: rename to [set_]first / ...
    struct act_int {
        static ruleT first(const subsetT& subset, const maskT& mask, const moldT& mold) {
            assert(compatible(subset, mold));

            return transform(subset, mask, mold, [](bool* begin, bool* end) { std::fill(begin, end, 0); });
        }

        static ruleT last(const subsetT& subset, const maskT& mask, const moldT& mold) {
            assert(compatible(subset, mold));

            return transform(subset, mask, mold, [](bool* begin, bool* end) { std::fill(begin, end, 1); });
        }

        // TODO: next/prev should require `contains`... using compatible to make the gui part work.
        static ruleT next(const subsetT& subset, const maskT& mask, const moldT& mold) {
            // assert(subset.contains(mold.rule));
            assert(compatible(subset, mold));

            return transform(subset, mask, mold, [](bool* begin, bool* end) {
                // 11...0 -> 00...1; stop at 111...1 (last())
                bool* first_0 = std::find(begin, end, 0);
                if (first_0 != end) {
                    std::fill(begin, first_0, 0);
                    *first_0 = 1;
                }
            });
        }

        static ruleT prev(const subsetT& subset, const maskT& mask, const moldT& mold) {
            // assert(subset.contains(mold.rule));
            assert(compatible(subset, mold));

            return transform(subset, mask, mold, [](bool* begin, bool* end) {
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
        // TODO: should there be "set first c to be 1 and rest to be 0" util in the gui?

        static ruleT first(const subsetT& subset, const maskT& mask, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return transform(subset, mask, mold, [](bool* begin, bool* end) {
                int c = std::count(begin, end, 1);
                std::fill(begin, end, 0);
                std::fill_n(begin, c, 1);
            });
        }

        static ruleT last(const subsetT& subset, const maskT& mask, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return transform(subset, mask, mold, [](bool* begin, bool* end) {
                int c = std::count(begin, end, 1);
                std::fill(begin, end, 0);
                std::fill_n(end - c, c, 1);
            });
        }

        static ruleT next(const subsetT& subset, const maskT& mask, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return transform(subset, mask, mold, [](bool* begin, bool* end) {
                // Stop at 000...111 (last())
                if (std::find(std::find(begin, end, 1), end, 0) == end) {
                    return;
                }

                std::next_permutation(std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
            });
        }

        static ruleT prev(const subsetT& subset, const maskT& mask, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return transform(subset, mask, mold, [](bool* begin, bool* end) {
                // Stop at 111...000 (first())
                if (std::find(std::find(begin, end, 0), end, 1) == end) {
                    return;
                }

                std::prev_permutation(std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
            });
        }
    };

    // TODO: these transforms can take advantage of _make_subset utilities. (but may not at cost of less clarity)
    // TODO: proper name...
    inline moldT trans_mirror(const moldT& mold) {
        moldT mir{};
        for_each_code([&](codeT code) {
            const auto [q, w, e, a, s, d, z, x, c] = decode(code);
            const codeT code_mir = encode({!q, !w, !e, //
                                           !a, !s, !d, //
                                           !z, !x, !c});
            mir.lock[code_mir] = mold.lock[code];
            mir.rule[code_mir] = (mold.rule[code] == s) ? !s : !(!s); // So that ->
            assert((mir.rule[code_mir] == !s) == (mold.rule[code] == s));
        });
        return mir;
    }

    // TODO: add test that mirror->mirror->original...

    // TODO: this is correct, but will open door to unsupported subsets
    // (For example, rules in `hex` space will be mapped to `hex2` space, so all the symmetry checks are lost)
    inline moldT trans_left_right(const moldT& mold) {
        moldT lr{};
        for_each_code([&](codeT code) {
            const auto [q, w, e, a, s, d, z, x, c] = decode(code);
            const codeT code_lr = encode({e, w, q, //
                                          d, s, a, //
                                          c, x, z});
            lr.lock[code_lr] = mold.lock[code];
            lr.rule[code_lr] = mold.rule[code];
        });
        return lr;
    }

    inline namespace _make_subset {
        // TODO: better name ("make_mask" is too-general name)...
        inline maskT make_mask(codeT::bposE bpos) {
            return {make_rule([bpos](codeT code) { return code.get(bpos); })};
        }

        // TODO: explain the effects of these mask...
        // rule ^ mask_zero -> TODO
        inline const maskT mask_zero{{}};
        // rule ^ mask_identity -> TODO
        inline const maskT mask_identity{make_mask(codeT::bpos_s)};
        // TODO: mask_copy_q/w/e/a/s(~mask_identity)/d/z/x/c etc?

        // A mapperT defines a rule that maps each codeT to another codeT.
        // Specifically, mapperT{"qweasdzxc"} maps any codeT to the same value.
        class mapperT {
            struct takeT {
                enum tagE { O, I, Get, NGet };
                tagE tag;
                codeT::bposE bpos;
                bool operator()(codeT code) const {
                    switch (tag) {
                        case O: return 0;
                        case I: return 1;
                        case Get: return code.get(bpos);
                        default: assert(tag == NGet); return !code.get(bpos);
                    }
                }
            };

            // TODO: better name...
            takeT q, w, e;
            takeT a, s, d;
            takeT z, x, c;

        public:
            // TODO: about consteval and the (obsolete) plan to support user-defined mappers / subsets...
            consteval mapperT(const char* str) {
                // [01]|!?[qweasdzxc]
                auto parse = [&]() -> takeT {
                    takeT::tagE tag = takeT::Get;
                    switch (*str) {
                        case '0': ++str; return {takeT::O, {}};
                        case '1': ++str; return {takeT::I, {}};
                        case '!':
                            ++str;
                            tag = takeT::NGet;
                            break;
                    }
                    switch (*str++) {
                        case 'q': return {tag, codeT::bpos_q};
                        case 'w': return {tag, codeT::bpos_w};
                        case 'e': return {tag, codeT::bpos_e};
                        case 'a': return {tag, codeT::bpos_a};
                        case 's': return {tag, codeT::bpos_s};
                        case 'd': return {tag, codeT::bpos_d};
                        case 'z': return {tag, codeT::bpos_z};
                        case 'x': return {tag, codeT::bpos_x};
                        case 'c': return {tag, codeT::bpos_c};
                        default: throw 0;
                    }
                };
                q = parse(), w = parse(), e = parse();
                a = parse(), s = parse(), d = parse();
                z = parse(), x = parse(), c = parse();
                if (*str != '\0') {
                    throw 0;
                }
            }

            codeT operator()(codeT code) const {
                return encode({q(code), w(code), e(code), //
                               a(code), s(code), d(code), //
                               z(code), x(code), c(code)});
            }
        };

        // A pair of mapperT defines an equivalence relation.
        struct mapperT_pair {
            mapperT a, b;
        };

        inline void add_eq(equivT& eq, const mapperT_pair& mp) {
            for_each_code([&](codeT code) { eq.add_eq(mp.a(code), mp.b(code)); });
        }

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
                                              "zxc"); // swap(q,w); *C8 -> totalistic, excluding s
        inline constexpr mapperT mp_tot_inc_s("qse"
                                              "awd"
                                              "zxc"); // swap(w,s); *C8 -> totalistic, including s

        // TODO: explain. TODO: better name...
        inline constexpr mapperT mp_dual("!q!w!e"
                                         "!a!s!d"
                                         "!z!x!c");

        // Hexagonal emulation and emulated symmetry.

        // q w -     q w
        // a s d -> a s d
        // - x c     x c
        inline constexpr mapperT mp_hex_ignore("qw0"
                                               "asd"
                                               "0xc"); // ignore_(e,z)

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
                                                  "0xc"); // swap(q,w); *C6 -> totalistic, excluding s
        inline constexpr mapperT mp_hex_tot_inc_s("qs0"
                                                  "awd"
                                                  "0xc"); // swap(w,s); *C6 -> totalistic, including s

#if 0
        // It's also valid to emulate hexagonal neighborhood by ignoring "q" and "c".
        // However, the program is not going to support this, as it makes the program more complicated
        // but without much benefit.

        // - w e     w e
        // a s d -> a s d
        // z x -     z x
        inline constexpr mapperT mp_hex2_ignore("0we"
                                                "asd"
                                                "zx0"); // ignore_(q,c)
#endif

        inline subsetT make_subset(std::initializer_list<mapperT> mappers, const maskT& mask = mask_zero) {
            equivT eq{};
            for (const mapperT& m : mappers) {
                add_eq(eq, {mp_identity, m});
            }
            return subsetT{mask, eq};
        }

    } // namespace _make_subset

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_mappers = [] {
            for_each_code([](codeT code) {
                assert(mp_identity(code) == code);

                for (const mapperT* m : {&mp_refl_asd, &mp_refl_wsx, &mp_refl_qsc, &mp_refl_esz, &mp_C2}) {
                    assert((*m)((*m)(code)) == code);
                }
                assert(mp_C4(mp_C4(mp_C4(mp_C4(code)))) == code);

                assert(mp_dual(mp_dual(code)) == code);

                // ... TODO...
            });
        };

        inline const testT test_subset_intersection = [] {
            subsetT sc = make_subset({mp_ignore_s}, mask_zero) & make_subset({mp_dual}, mask_identity);

            // 2024/1/20 2AM
            // There is NO problem in the algorithm.
            // It's just that, in this situation the maskT has a strong bias, so that it's too easy to generate
            // rules in a certain direction...
            using enum codeT::bposE;
            assert(!sc.empty());
            assert(sc.contains(make_mask(bpos_q)));
            assert(sc.contains(make_mask(bpos_w)));
            assert(sc.contains(make_mask(bpos_e)));
            assert(sc.contains(make_mask(bpos_a)));
            assert(!sc.contains(mask_identity)); // Doesn't contain make_mask(bpos_s).
            assert(sc.contains(make_mask(bpos_d)));
            assert(sc.contains(make_mask(bpos_z)));
            assert(sc.contains(make_mask(bpos_x)));
            assert(sc.contains(make_mask(bpos_c)));
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

} //  namespace legacy
