#pragma once

#include <map>
#include <optional>

#include "rule.hpp"

// higher-order(???) ...TODO...
namespace legacy {
    // TODO: better variable names...
    class partitionT {
    public:
        using array_base = array<int, 512>;

    private:
        array_base m_map;
        int m_k;
        vector<vector<int>> m_groups;

        // TODO: remove in the future...
        void invariants() {
            assert(map(0) == 0);
            int max_sofar = 0; // part[0].
            for (auto gcode : m_map) {
                if (gcode > max_sofar) {
                    assert(gcode == max_sofar + 1);
                    max_sofar = gcode;
                }
            }
            assert(max_sofar + 1 == m_k);
        }

    public:
        /* implicit */ partitionT(const array_base& part) : m_map{part} {
            struct {
                std::map<int, int> mapper;
                int next = 0;
                int operator()(int c) {
                    if (!mapper.contains(c)) {
                        mapper[c] = next++;
                    }
                    return mapper[c];
                }
            } regulate;

            for (auto& gcode : m_map) {
                gcode = regulate(gcode);
            }

            // regulated.
            m_k = regulate.next;
            m_groups.resize(m_k);
            for (int code = 0; code < 512; ++code) {
                m_groups[m_map[code]].push_back(code);
            }
        }

        int map(int code) const {
            assert(code >= 0 && code < 512);
            return m_map[code];
        }
        const vector<vector<int>>& groups() const {
            return m_groups;
        }
        const vector<int>& group_for(int code) const {
            return m_groups[map(code)];
        }
        int head_for(int code) const {
            return m_groups[map(code)][0];
        }

        int k() const {
            return m_k;
        }

        // TODO: rename variable...
        // TODO: must be member function?
        ruleT_base gather_from(const ruleT_base& rule) const {
            ruleT_base grule{};
            for (int j = 0; j < k(); ++j) {
                grule[j] = rule[m_groups[j][0]];
            }
            return grule;
        }

        ruleT_base dispatch_from(const ruleT_base& grule) const {
            ruleT_base rule;
            for (int code = 0; code < 512; ++code) {
                rule[code] = grule[map(code)];
            }
            return rule;
        }

        // TODO: bad name...
        bool matches(const ruleT_base& rule) const {
            for (int code = 0; code < 512; ++code) {
                if (rule[code] != rule[head_for(code)]) {
                    return false;
                }
            }
            return true;
        }

        bool subdivides(const partitionT& part) const {
            for (const auto& group : m_groups) {
                for (auto code : group) {
                    if (part.m_map[code] != part.m_map[group[0]]) {
                        return false;
                    }
                }
            }
            return true;
        }
    };

    namespace partition {
        // TODO: none/orthogonal/diagonal are too noisy; only support when configured...
        enum basic_specification : int {
            // none = 0,
            // orthogonal,
            // diagonal,
            spatial,
            ro45,
            spatial_ro45,
            permutation,
            size
        };
        static constexpr const char* basic_specification_names[]{/*"none",    "orthogonal",   "diagonal",   */
                                                                 "spatial", "ro45", "spatial_ro45", "permutation"};
        inline namespace s {
            // TODO: currently in a new namespace to avoid enumrator clash...
            enum extra_specification : int { none = 0, paired, state, size };
        } // namespace s

        static constexpr const char* extra_specification_names[]{"basic", "paired", "state"};

        inline const partitionT& get_partition(basic_specification basic, extra_specification extr) {
            using mapperP = int (*)(int);

            constexpr auto make_partition = [](const vector<mapperP>& mappers) -> partitionT {
                partitionT::array_base part;
                part.fill(-1);

                auto equiv = [&](int code, int color, auto& equiv) -> void {
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
                for (int code = 0; code < 512; code++) {
                    if (part[code] == -1) {
                        equiv(code, color++, equiv);
                    }
                }
                return part;
            };

#define mapto(...)                                                                                                     \
    +[](int code) {                                                                                                    \
        auto [q, w, e, a, s, d, z, x, c] = decode(code);                                                               \
        return encode(__VA_ARGS__);                                                                                    \
    }
            // z x c
            // a s d
            // q w e
            constexpr mapperP upside_down = mapto(z, x, c, a, s, d, q, w, e);
            // e w q
            // d s a
            // c x z
            constexpr mapperP leftside_right = mapto(e, w, q, d, s, a, c, x, z);
            // q a z
            // w s x
            // e d c
            constexpr mapperP main_diag = mapto(q, a, z, w, s, x, e, d, c);
            // c d e
            // x s w
            // z a q
            constexpr mapperP side_diag = mapto(c, d, e, x, s, w, z, a, q);
            // w e d
            // q s c
            // a z x
            constexpr mapperP rotate_45 = mapto(w, e, d, q, s, c, a, z, x);
            // q w e
            // a !s d
            // z x c
            constexpr mapperP s_flipped = mapto(q, w, e, a, !s, d, z, x, c);
            // !q !w !e
            // !a !s !d
            // !z !x !c
            constexpr mapperP all_flipped = mapto(!q, !w, !e, !a, !s, !d, !z, !x, !c);
            // w q e
            // a s d
            // z x c
            constexpr mapperP perm_specific = mapto(w, q, e, a, s, d, z, x, c);
#undef mapto

            static const std::initializer_list<mapperP> args[basic_specification::size]{
                /*
                {},                                                     // none.
                {upside_down, leftside_right},                          // orthogonal
                {main_diag, side_diag},                                 // diagonal
                */
                {upside_down, leftside_right, main_diag /*side_diag*/}, // spatial
                {rotate_45},                                            // ro45
                {upside_down, leftside_right, main_diag, rotate_45},    // spatial_ro45
                {rotate_45, perm_specific}                              // permutation
            };
            static std::optional<partitionT> parts[basic_specification::size][extra_specification::size];
            // apparently not thread-safe... TODO: should be thread safe or not?
            if (!parts[basic][extr]) {
                vector<mapperP> arg = args[basic];
                switch (extr) {
                case extra_specification::none:
                    break;
                case extra_specification::paired:
                    arg.push_back(s_flipped);
                    break;
                case extra_specification::state:
                    arg.push_back(all_flipped);
                    break;
                default:
                    abort();
                }
                parts[basic][extr].emplace(make_partition(arg));
            }
            return *parts[basic][extr];
        }

        inline const partitionT& get_partition_spatial() {
            return get_partition(basic_specification::spatial, extra_specification::none);
        }
    } // namespace partition

} // namespace legacy
