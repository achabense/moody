#pragma once

#include <map>
#include <optional>

#include "rule.hpp"

// TODO: this header shall be redesigned... pending partition_v2...

// 2023/12/3
// I've had great difficulty reasoning about "constrained randomizer".
// Currently, I want to write down the following interface:
// | randomize(const interT& [inter],const partitionT& [p],int [count],const partialT& [constraint])
// | return a rule that:
// | viewed by [inter], there should be [count] groups in [p] flipped on, with the exception that, all
// | for a code [c] , if [constraint].c is not unknown, then rule[c] must be of that value.
// So, if some [c] in [constraint] is known, then the group should not be flipped. Then,
// Fuck I cannot describe that well...

namespace legacy {
    // TODO: reconsider what can be "valid interpretation" of ruleT_data...

    using ruleT_data = ruleT::data_type;

    // TODO: explain...
    // The data in `ruleT` has fixed meaning - the value `s` is mapped to.
    // However, it is not the only natural interpretation of the data.
    // For example, it's also meaningful to interpret the data as - whether `s` is "flipped"?
    // ...

    // TODO: is arbitrary `Diff` really well-formed?
    struct interT {
        enum tagE : int { Value, Flip, Diff };
        tagE tag = Value;
        ruleT custom{};

        const ruleT& get_viewer() const { return get_viewer(tag); }

        const ruleT& get_viewer(tagE tag) const {
            static constexpr ruleT zero{};
            static constexpr ruleT identity = mkrule(decode_s);

            switch (tag) {
            case Value:
                return zero;
            case Flip:
                return identity;
            case Diff:
                return custom;
            default:
                abort();
            }
        }

        // both methods are actually XOR...
        ruleT_data from_rule(const ruleT& rule) const {
            const ruleT& base = get_viewer();
            ruleT_data diff{};
            for (codeT code : codeT{}) {
                diff[code] = rule(code) == base(code) ? 0 : 1;
            }
            return diff;
        }

        ruleT to_rule(const ruleT_data& diff) const {
            const ruleT& base = get_viewer();
            ruleT rule{};
            for (codeT code : codeT{}) {
                rule.map[code] = diff[code] ? !base(code) : base(code);
            }
            return rule;
        }
    };

    using groupT = std::vector<codeT>;

    // TODO: emphasis that a partitionT is supposed to hold [semantics]???

    // TODO: using gcodeT (or other names) = int;
    // TODO: better variable names...
    class partitionT {
    public:
        using array_base = std::array<int, 512>;

    private:
        array_base m_map;
        int m_k;
        std::vector<groupT> m_groups;

        static int regulate(array_base& data) {
            std::map<int, int> mapper;
            int next = 0;
            for (auto& gcode : data) {
                if (!mapper.contains(gcode)) {
                    mapper[gcode] = next++;
                }
                gcode = mapper[gcode];
            }
            assert(next == mapper.size());
            return next;
        }

    public:
        /* implicit */ partitionT(const array_base& part) : m_map{part} {
            m_k = regulate(m_map);

            // regulated.
            m_groups.resize(m_k);

            // TODO: temporary; should be dealt with by getp...
            // (this part should be redesigned)
            for (codeT code : codeT{}) {
                m_groups[m_map[code]].push_back(code);
            }
#if 0
            const bool paired = m_map[0] == m_map[16];
            const bool state = m_map[0] == m_map[511];
            if (paired || state) {
                for (auto& group : m_groups) {
                    const auto pred = [f = group[0]](codeT code) { return decode_s(code) == decode_s(f); };
                    const auto mid = std::stable_partition(group.begin(), group.end(), pred);
                    if (state) {
                        std::reverse(mid, group.end());
                    }
                }
            }
#endif
        }

        int map(codeT code) const {
            assert(code >= 0 && code < 512);
            return m_map[code];
        }
        const std::vector<groupT>& groups() const { //
            return m_groups;
        }
        const groupT& group_for(codeT code) const { //
            return m_groups[map(code)];
        }

        int k() const { return m_k; }

        // TODO: the arg type is problematic.
        ruleT_data dispatch_from(const ruleT_data& grule) const {
            ruleT_data rule{}; // TODO: without {}, msvc gives false positive after applying range-for...
            for (codeT code : codeT{}) {
                rule[code] = grule[map(code)];
            }
            return rule;
        }

        // TODO: bad name...
        bool matches(const ruleT_data& rule) const {
            for (codeT code : codeT{}) {
                const codeT head = group_for(code)[0];
                if (rule[code] != rule[head]) {
                    return false;
                }
            }
            return true;
        }

        bool subdivides(const partitionT& part) const {
            for (const auto& group : m_groups) {
                for (codeT code : group) {
                    if (part.m_map[code] != part.m_map[group[0]]) {
                        return false;
                    }
                }
            }
            return true;
        }

        // TODO: technically not needing regulation, as long as e.g. int~i32 and use i64 map for bit concatenation...
        // (And this is not actually needed in the app...)
        static partitionT pand(const partitionT& a, const partitionT& b) {
            array_base p{};
            for (codeT code : codeT{}) {
                p[code] = a.m_map[code] | (b.m_map[code] << 10);
            }
            return p;
        }

        // TODO: more formal name?
        // TODO: time-complexity? (<=512*512, so not important...)
        static partitionT lcm(const partitionT& a, const partitionT& b) {
            array_base p{};
            p.fill(-1);
            auto equiv = [&](codeT code, int color, auto& equiv) -> void {
                assert(color != -1);
                if (p[code] != -1) {
                    assert(p[code] == color);
                    return;
                }
                p[code] = color;
                for (codeT c : a.group_for(code)) {
                    equiv(c, color, equiv);
                }
                for (codeT c : b.group_for(code)) {
                    equiv(c, color, equiv);
                }
            };

            int color = 0;
            for (codeT code : codeT{}) {
                if (p[code] == -1) {
                    equiv(code, color++, equiv);
                }
            }
            return p;
        }

        enum basespecE : int { //
            None = 0,
            Orthogonal,
            Diagonal,
            Spatial,
            Ro45,
            Spatial_ro45,
            Permutation,
            basespecE_size
        };
        static constexpr const char* basespecE_names[]{"(deprecated) none",       //
                                                       "(deprecated) orthogonal", //
                                                       "(deprecated) diagonal",   //
                                                       "spatial",
                                                       "ro45",
                                                       "spatial_ro45",
                                                       "permutation"};

        // TODO: explain...
        enum extrspecE : int { //
            None_ = 0,         // Are there logical reasons why C++ doesn't allow "None" here?
            Paired,
            State,
            extrspecE_size
        };
        static constexpr const char* extrspecE_names[]{"none", "paired", "state"};

        static const partitionT& getp(basespecE basic, extrspecE extr) {
            using mapperP = codeT (*)(codeT);

            constexpr auto make_partition = [](const std::vector<mapperP>& mappers) -> partitionT {
                array_base part;
                part.fill(-1);

                auto equiv = [&](codeT code, int color, auto& equiv) -> void {
                    assert(color != -1);
                    if (part[code] != -1) {
                        assert(part[code] == color);
                        return;
                    }
                    part[code] = color;
                    for (auto mapper : mappers) {
                        equiv(mapper(code), color, equiv);
                    }
                };

                int color = 0;
                for (codeT code : codeT{}) {
                    if (part[code] == -1) {
                        equiv(code, color++, equiv);
                    }
                }
                return part;
            };

            // TODO: very strange format...
#define MAPTO(...)                                       \
    +[](codeT code) {                                    \
        auto [q, w, e, a, s, d, z, x, c] = decode(code); \
        return encode(__VA_ARGS__);                      \
    }
            // z x c
            // a s d
            // q w e
            constexpr mapperP upside_down = MAPTO(z, x, c, a, s, d, q, w, e);
            // e w q
            // d s a
            // c x z
            constexpr mapperP leftside_right = MAPTO(e, w, q, d, s, a, c, x, z);
            // q a z
            // w s x
            // e d c
            constexpr mapperP main_diag = MAPTO(q, a, z, w, s, x, e, d, c);
            // c d e
            // x s w
            // z a q
            constexpr mapperP side_diag = MAPTO(c, d, e, x, s, w, z, a, q);
            // w e d
            // q s c
            // a z x
            constexpr mapperP rotate_45 = MAPTO(w, e, d, q, s, c, a, z, x);
            // w q e
            // a s d
            // z x c
            constexpr mapperP perm_specific = MAPTO(w, q, e, a, s, d, z, x, c);
#if 0
            // 1. I misunderstood "rotate" symmetry. "ro45" is never about symmetry (I've no idea what it is)
            // 2. As seemingly-senseless partition like ro45 can make non-trivial patterns, should support after all...
            // edc qwe
            // wsx asd
            // qaz zxc
            constexpr mapperP rotate_sq =
                MAPTO(e, d, c, w, s, x, q, a,
                      z); // Only this is what rect space can have... rotate_45 is really not about symmetry.
            // qed
            // wsx
            // azc skip q,c.
            constexpr mapperP rotate_6 = MAPTO(q, e, d, w, s, x, a, z, c);
            // constexpr mapperP perm_6 = MAPTO(q, e, w, a, s, d, z, x, c);

            constexpr auto make_partition_mask = [](bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x,
                                                    bool c) -> partitionT {
                codeT mask = encode(q, w, e, a, s, d, z, x, c);
                partitionT::array_base p{};
                for (codeT code : codeT{}) {
                    p[code] = code & mask;
                }
                return p;
            };

            static partitionT msk = make_partition_mask(0, 1, 1, 1, 1, 1, 1, 1, 0);
            static partitionT r6 = make_partition({rotate_6});
            static partitionT res = lcm(msk, r6);
            return res;
#endif
#undef MAPTO

            static const std::initializer_list<mapperP> args[basespecE_size]{
                {},                                                     // none
                {upside_down, leftside_right},                          // orthogonal
                {main_diag, side_diag},                                 // diagonal
                {upside_down, leftside_right, main_diag /*side_diag*/}, // spatial
                {rotate_45},                                            // ro45
                {upside_down, leftside_right, main_diag, rotate_45},    // spatial_ro45
                {rotate_45, perm_specific}                              // permutation
            };
            static std::optional<partitionT> parts[basespecE_size][extrspecE_size];
            // apparently not thread-safe... TODO: should be thread safe or not?
            if (!parts[basic][extr]) {
                std::vector<mapperP> arg = args[basic];
                switch (extr) {
                case extrspecE::None_:
                    break;
                case extrspecE::Paired:
                    arg.push_back(flip_s);
                    break;
                case extrspecE::State:
                    arg.push_back(flip_all);
                    break;
                default:
                    abort();
                }
                parts[basic][extr].emplace(make_partition(arg));
            }
            return *parts[basic][extr];
        }

        // TODO: better name...
        static const partitionT& get_spatial() { return getp(Spatial, extrspecE::None_); }
    };

    class scanlistT {
    public:
        enum scanE : char { A0, A1, Inconsistent };

    private:
        std::array<scanE, 512> m_data; // TODO: vector?
        int m_k;
        int m_count[3];

    public:
        static scanE scan(const groupT& group, const ruleT_data& rule) {
            bool has[2]{};
            for (codeT code : group) {
                if (has[!rule[code]]) {
                    return Inconsistent;
                }
                has[rule[code]] = true;
            }
            return has[0] ? A0 : A1;
        }

        scanlistT(const partitionT& p, const ruleT_data& rule) : m_data{}, m_k{p.k()}, m_count{} {
            for (int j = 0; j < m_k; ++j) {
                m_data[j] = scan(p.groups()[j], rule);
                ++m_count[m_data[j]];
            }
        }

        auto begin() const { return m_data.cbegin(); }
        auto end() const { return m_data.cbegin() + m_k; }
        const scanE& operator[](int j) const { return m_data[j]; }

        int k() const { return m_k; }
        int count(scanE s) const { return m_count[s]; }
    };

    // TODO: rename...
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
    // TODO: generate randomizer based on partialT and partitionT...
} // namespace legacy

// TODO: temp...
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
