#pragma once

#include <map>

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

    inline partitionT make_partition(std::initializer_list<int (*)(int)> mappers) {
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
    }

    namespace partition {
        // TODO: horribly wasteful...
        inline const partitionT none = [] {
            partitionT::array_base part;
            for (int code = 0; code < 512; ++code) {
                part[code] = code;
            }
            return part;
        }();

#define mapto(...)                                                                                                     \
    +[](int code) {                                                                                                    \
        auto [q, w, e, a, s, d, z, x, c] = decode(code);                                                               \
        return encode(__VA_ARGS__);                                                                                    \
    }

        // z x c
        // a s d
        // q w e
        inline constexpr auto upside_down = mapto(z, x, c, a, s, d, q, w, e);
        // e w q
        // d s a
        // c x z
        inline constexpr auto leftside_right = mapto(e, w, q, d, s, a, c, x, z);
        // q a z
        // w s x
        // e d c
        inline constexpr auto main_diag = mapto(q, a, z, w, s, x, e, d, c);
        // c d e
        // x s w
        // z a q
        inline constexpr auto side_diag = mapto(c, d, e, x, s, w, z, a, q);
        // w e d
        // q s c
        // a z x
        inline constexpr auto rotate_45 = mapto(w, e, d, q, s, c, a, z, x);
        // q w e
        // a !s d
        // z x c
        inline constexpr auto s_flipped = mapto(q, w, e, a, !s, d, z, x, c);
        // !q !w !e
        // !a !s !d
        // !z !x !c
        inline constexpr auto all_flipped = mapto(!q, !w, !e, !a, !s, !d, !z, !x, !c);

        inline const partitionT sub_spatial = make_partition({upside_down, leftside_right});

        inline const partitionT sub_spatial2 = make_partition({main_diag, side_diag});

        // spatial-symmetric.
        inline const partitionT spatial = make_partition({upside_down, leftside_right, main_diag});

        inline const partitionT spatial_paired = make_partition({upside_down, leftside_right, main_diag, s_flipped});

        // TODO: explain.
        inline const partitionT spatial_state = make_partition({upside_down, leftside_right, main_diag, all_flipped});

        inline const partitionT ro45_only = make_partition({rotate_45});
        inline const partitionT spatial_ro45 = make_partition({upside_down, leftside_right, main_diag, rotate_45});

        inline const partitionT permutation = [] {
            partitionT::array_base part;
            for (int code = 0; code < 512; ++code) {
                // clang-format off
            auto [q, w, e,
                  a, s, d,
                  z, x, c] = decode(code);
                // clang-format on
                part[code] = q + w + e + a + d + z + x + c + (s ? 100 : 0);
            }
            return part;
        }();
    } // namespace partition
    // TODO: sub-namespace...

} // namespace legacy
