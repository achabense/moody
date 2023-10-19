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

        ruleT::array_base dispatch_from(const ruleT::array_base& grule) const {
            ruleT::array_base rule;
            for (int code = 0; code < 512; ++code) {
                rule[code] = grule[map(code)];
            }
            return rule;
        }

        bool matches(const ruleT& rule) const {
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
        // TODO: horribly wasteful...
        inline const partitionT none = [] {
            partitionT::array_base part;
            for (int code = 0; code < 512; ++code) {
                part[code] = code;
            }
            return part;
        }();

        // TODO: spatial dropped diagonal... what does this symmetric mean?
        inline const partitionT sub_spatial = [] {
            partitionT::array_base part;
            part.fill(-1);

            auto equiv = [&](int code, int color, auto& equiv) -> void {
                if (part[code] != -1) {
                    assert(part[code] == color);
                    return;
                }
                part[code] = color;
                // clang-format off
            auto [q, w, e,
                  a, s, d,
                  z, x, c] = decode(code);
            equiv(encode(z, x, c,
                         a, s, d,
                         q, w, e), color, equiv); // upside-down
            equiv(encode(e, w, q,
                         d, s, a,
                         c, x, z), color, equiv); // "leftside-right"
                // clang-format on
            };

            int color = 0;
            for (int code = 0; code < 512; code++) {
                if (part[code] == -1) {
                    equiv(code, color++, equiv);
                }
            }
            return part;
        }();

        inline const partitionT sub_spatial2 = [] {
            partitionT::array_base part;
            part.fill(-1);

            auto equiv = [&](int code, int color, auto& equiv) -> void {
                if (part[code] != -1) {
                    assert(part[code] == color);
                    return;
                }
                part[code] = color;
                // clang-format off
            auto [q, w, e,
                  a, s, d,
                  z, x, c] = decode(code);
            equiv(encode(q, a, z,
                         w, s, x,
                         e, d, c), color, equiv); // main-diag.
            equiv(encode(c, d, e,
                         x, s, w,
                         z, a, q), color, equiv); // side-diag.
                // clang-format on
            };

            int color = 0;
            for (int code = 0; code < 512; code++) {
                if (part[code] == -1) {
                    equiv(code, color++, equiv);
                }
            }
            return part;
        }();

        // basic symmetric partition.
        inline const partitionT spatial = [] {
            partitionT::array_base part;
            part.fill(-1);

            auto equiv = [&](int code, int color, auto& equiv) -> void {
                if (part[code] != -1) {
                    assert(part[code] == color);
                    return;
                }
                part[code] = color;
                // clang-format off
            auto [q, w, e,
                  a, s, d,
                  z, x, c] = decode(code);
            equiv(encode(z, x, c,
                         a, s, d,
                         q, w, e), color, equiv); // upside-down
            equiv(encode(e, w, q,
                         d, s, a,
                         c, x, z), color, equiv); // "leftside-right"
            equiv(encode(q, a, z,
                         w, s, x,
                         e, d, c), color, equiv); // diagonal
                // clang-format on
            };

            int color = 0;
            for (int code = 0; code < 512; code++) {
                if (part[code] == -1) {
                    equiv(code, color++, equiv);
                }
            }
            return part;
        }();

        inline const partitionT spatial_paired = [] {
            partitionT::array_base part;
            part.fill(-1);

            auto equiv = [&](int code, int color, auto& equiv) -> void {
                if (part[code] != -1) {
                    assert(part[code] == color);
                    return;
                }
                part[code] = color;
                // clang-format off
            auto [q, w, e,
                  a, s, d,
                  z, x, c] = decode(code);
            equiv(encode(z, x, c,
                         a, s, d,
                         q, w, e), color, equiv); // upside-down
            equiv(encode(e, w, q,
                         d, s, a,
                         c, x, z), color, equiv); // "leftside-right"
            equiv(encode(q, a, z,
                         w, s, x,
                         e, d, c), color, equiv); // diagonal
            // tie...
            equiv(encode(q, w, e,
                         a, !s, d,
                         z, x, c), color, equiv);
                // clang-format on
            };

            int color = 0;
            for (int code = 0; code < 512; code++) {
                if (part[code] == -1) {
                    equiv(code, color++, equiv);
                }
            }
            return part;
        }();

        // TODO: explain.
        // a special rule that...
        inline const partitionT spatial_state = [] {
            partitionT::array_base part;
            part.fill(-1);

            auto equiv = [&](int code, int color, auto& equiv) -> void {
                if (part[code] != -1) {
                    assert(part[code] == color);
                    return;
                }
                part[code] = color;
                // clang-format off
            auto [q, w, e,
                  a, s, d,
                  z, x, c] = decode(code);
            equiv(encode(z, x, c,
                         a, s, d,
                         q, w, e), color, equiv); // upside-down
            equiv(encode(e, w, q,
                         d, s, a,
                         c, x, z), color, equiv); // "leftside-right"
            equiv(encode(q, a, z,
                         w, s, x,
                         e, d, c), color, equiv); // diagonal
            // TODO: this is the only difference...
            equiv(encode(!q, !w, !e,
                         !a, !s, !d,
                         !z, !x, !c), color, equiv);
                // clang-format on
            };

            int color = 0;
            for (int code = 0; code < 512; code++) {
                if (part[code] == -1) {
                    equiv(code, color++, equiv);
                }
            }
            return part;
        }();

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
