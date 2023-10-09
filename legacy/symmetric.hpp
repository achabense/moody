#pragma once

#include <map>
#include <optional>

#include "rule.hpp"

// higher-order ...TODO...
namespace legacy {
    using partitionT = array<int, 512>;

    class regulated_partition {
        partitionT m_map;
        int m_k;

        mutable std::optional<vector<vector<int>>> m_groups;

    public:
        /* implicit */ regulated_partition(const partitionT& partition) : m_map{partition} {
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

            for (auto& pcode : m_map) {
                pcode = regulate(pcode);
            }

            m_k = regulate.next;

#ifndef NDEBUG
            assert(m_map[0] == 0);
            int max_sofar = 0; // part[0].
            for (auto pcode : m_map) {
                if (pcode > max_sofar) {
                    assert(pcode == max_sofar + 1);
                    max_sofar = pcode;
                }
            }
            assert(max_sofar + 1 == m_k);
#endif
        }

        // TODO: what does this imply to coding?
        /* implicit */ operator const partitionT&() const {
            return m_map;
        }

        int k() const {
            return m_k;
        }

        // TODO: ?...
        int map(int code) const {
            return m_map[code];
        }

        // TODO: matches_as_???
        [[nodiscard]] bool matches(const ruleT& rule, bool as_flip = false) {
            enum state : int { unknown = -1, isfalse = false, istrue = true };
            array<state, 512 /* >=m_k */> seen;
            seen.fill(unknown);
            for (int code = 0; code < 512; ++code) {
                bool should_be;
                if (!as_flip) {
                    should_be = rule[code];
                } else {
                    bool s = (code >> 4) & 1;
                    should_be = s == rule[code] ? false : true;
                }

                int pcode = m_map[code];
                if (seen[pcode] == unknown) {
                    seen[pcode] = (state)should_be;
                } else if (seen[pcode] != should_be) {
                    return false;
                }
            }
            return true;
        }

        // NOTICE: not thread-safe.
        [[nodiscard]] bool subdivides(const partitionT& part) const {
            for (const auto& group : groups()) {
                for (auto code : group) {
                    if (part[code] != part[group[0]]) {
                        return false;
                    }
                }
            }
            return true;
        }

        // NOTICE: not thread-safe.
        // TODO: should I emphasis on this?
        [[nodiscard]] const vector<vector<int>>& groups() const {
            if (!m_groups.has_value()) {
                vector<vector<int>> groups(m_k);
                for (int code = 0; code < 512; ++code) {
                    int pcode = m_map[code];
                    groups[pcode].push_back(code);
                }
                m_groups.emplace(std::move(groups));
            }
            return *m_groups;
        }
    };

    // basic symmetric partition.
    inline const regulated_partition symmetric_partition = [] {
        partitionT part;
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

    // TODO: explain.
    // a special rule that...
    inline const regulated_partition special_partition = [] {
        partitionT part;
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
            equiv(encode(a, q, w,
                         z, s, e,
                         x, c, d), color, equiv); // rotate
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

    inline const regulated_partition permutation_partition = [] {
        partitionT part;
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

    // TODO: their ignore-s versions..
    // ...

    // ...

    // symmetric rule.

    // symmetric but ...

    // perm

    // subgroups of  ...

    // TODO: wether symmetric...

} // namespace legacy
