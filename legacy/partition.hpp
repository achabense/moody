#pragma once

#include <map>
#include <optional>

#include "rule.hpp"

namespace legacy {
    using ruleT_data = ruleT::data_type;

    // TODO: explain...
    // TODO: refine... better names; consistently use Abc naming convention?
    struct interT {
        enum tagE : int { Value, Flip, Diff };
        tagE tag = Value;
        ruleT custom{};

        const ruleT& get_base() const {
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
            const ruleT& base = get_base();
            ruleT_data diff{};
            for (codeT code : codeT{}) {
                diff[code] = rule(code) == base(code) ? 0 : 1;
            }
            return diff;
        }

        ruleT to_rule(const ruleT_data& diff) const {
            const ruleT& base = get_base();
            ruleT rule{};
            for (codeT code : codeT{}) {
                rule.map[code] = diff[code] ? !base(code) : base(code);
            }
            return rule;
        }
    };

    // TODO: using gcodeT (or other names) = int;
    // TODO: better variable names...
    class partitionT {
    public:
        using array_base = std::array<int, 512>;
        using groupT = std::vector<codeT>;

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
        codeT head_for(codeT code) const { //
            return group_for(code)[0];
        }

        int k() const { return m_k; }

        // TODO: refine partitionT methods...
        static void flip(const groupT& group, ruleT_data& rule) {
            for (codeT code : group) {
                rule[code] = !rule[code];
            }
        }

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
                if (rule[code] != rule[head_for(code)]) {
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
                partitionT::array_base part;
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
        // TODO: better names.
        enum scanE : int {
            Inconsistent,
            All_0, // TODO: A0, A1?
            All_1,
        };

    private:
        std::array<scanE, 512> m_data; // TODO: vector?
        int m_k;
        int m_count[3];

    public:
        static scanE scan(const partitionT::groupT& group, const ruleT_data& rule) {
            bool first = rule[group[0]];
            scanlistT::scanE r = first ? scanlistT::All_1 : scanlistT::All_0;
            for (codeT code : group) {
                if (rule[code] != first) {
                    r = scanlistT::Inconsistent;
                    break;
                }
            }
            return r;
        }

        explicit scanlistT(const partitionT& p, const ruleT_data& rule) : m_data{}, m_k{p.k()}, m_count{} {
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
} // namespace legacy

// TODO: in development...
namespace legacy {
    // TODO: whether to allow flip mode?
    struct modelT {
        // TODO: it's easy to define an invalid state, but how to adapt with partition?
        enum stateE : char { S0, S1, Unknown };
        std::array<stateE, 512> data;

        modelT() { reset(); }

        void set(codeT code, bool b) {
            // TODO: explain decision against invalid situ
            if (data[code] == Unknown) {
                data[code] = stateE{b};
            }
        }

        void reset() { data.fill(Unknown); }

        auto bind(const legacy::ruleT& rule) {
            // TODO: is this const?
            return [this, rule](codeT code) /*const*/ {
                set(code, rule(code));
                return rule(code);
            };
        }
    };

    inline std::vector<modelT::stateE> filter(const modelT& m, const partitionT& p) {
        std::vector<modelT::stateE> grouped(p.k(), modelT::Unknown);
        for (codeT code : codeT{}) {
            if (m.data[code] != modelT::Unknown) {
                grouped[p.map(code)] = m.data[code];
                // TODO: what if conflict?
            }
        }
        return grouped;
    }

    // TODO: temporary, should be redesigned.
    /*inline*/ ruleT make_rule(const modelT& m, const partitionT& p, int den, auto&& rand) {
        // step 1:
        std::vector<modelT::stateE> grouped = filter(m, p);

        // step 2:
        // TODO: doesn't make sense...
        for (auto& s : grouped) {
            if (s == modelT::Unknown) {
                s = ((rand() % 100) < den) ? modelT::S1 : modelT::S0;
            }
        }

        // step 3: dispatch...
        ruleT rule{};
        for (codeT code : codeT{}) {
            rule.map[code] = grouped[p.map(code)] == modelT::S0 ? 0 : 1;
        }
        return rule;
    }
} // namespace legacy
