#pragma once

#include <map>
#include <optional>

#include "rule.hpp"

// higher-order(???) ...TODO...
namespace legacy {
    // TODO: the main challenge is to provide user-interface...

    // TODO: using gcodeT (or other names) = int;
    // TODO: better variable names...
    class partitionT {
    public:
        using array_base = array<int, 512>;

    private:
        array_base m_map;
        int m_k;
        vector<vector<codeT>> m_groups;

        // TODO: remove in the future...
        void assert_invariants() {
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
            assert_invariants();
            m_groups.resize(m_k);
            for (codeT code = 0; code < 512; ++code) {
                if (!decode_s(code)) {
                    m_groups[m_map[code]].push_back(code);
                }
            }
            // TODO: temporary; should be dealt with by get_partition...
            bool paired =
                m_map[0] == m_map[16]; // TODO: not used; should recheck (this part should be totally redesigned)
            bool state = m_map[0] == m_map[511];
            if (!state) {
                for (codeT code = 0; code < 512; ++code) {
                    if (decode_s(code)) {
                        m_groups[m_map[code]].push_back(code);
                    }
                }
            } else {
                for (codeT code = 511; code >= 0; --code) {
                    if (decode_s(code)) {
                        m_groups[m_map[code]].push_back(code);
                    }
                }
            }
        }

        int map(codeT code) const {
            assert(code >= 0 && code < 512);
            return m_map[code];
        }
        const vector<vector<codeT>>& groups() const { //
            return m_groups;
        }
        const vector<codeT>& group_for(codeT code) const { //
            return m_groups[map(code)];
        }
        codeT head_for(codeT code) const { //
            return m_groups[map(code)][0];
        }

        int k() const { return m_k; }

        class scanlistT {
        public:
            // TODO: better names.
            enum scanE : int {
                Inconsistent,
                All_0,
                All_1,
            };

        private:
            std::array<scanE, 512> m_data; // TODO: vector?
            int m_k;

        public:
            explicit scanlistT(int k) : m_data{}, m_k(k) {}

            auto begin() const { return m_data.cbegin(); }
            auto end() const { return m_data.cbegin() + m_k; }

            int k() const { return m_k; }

            scanE& operator[](int j) { return m_data[j]; }
            const scanE& operator[](int j) const { return m_data[j]; }

            // TODO: cache, or count in ctor (need redesign)?
            int count(scanE s) const { //
                return std::count(begin(), end(), s);
            }
        };

        scanlistT scan(const ruleT_data& rule) const {
            scanlistT result(k());
            for (int j = 0; j < k(); ++j) {
                const auto& group = m_groups[j];
                bool first = rule[group[0]];
                result[j] = first ? result.All_1 : result.All_0;
                for (codeT code : group) {
                    if (rule[code] != first) {
                        result[j] = result.Inconsistent;
                        break;
                    }
                }
            }
            return result;
        }

        // TODO: the arg type is problematic.
        ruleT_data dispatch_from(const ruleT_data& grule) const {
            ruleT_data rule;
            for (codeT code = 0; code < 512; ++code) {
                rule[code] = grule[map(code)];
            }
            return rule;
        }

        // TODO: bad name...
        bool matches(const ruleT_data& rule) const {
            for (codeT code = 0; code < 512; ++code) {
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
    };

    namespace partition {
        // TODO: none/orthogonal/diagonal are too noisy; only support when configured...
        enum basic_specification : int {
            none = 0,
            orthogonal,
            diagonal,
            spatial,
            ro45,
            spatial_ro45,
            permutation,
            size
        };
        static constexpr const char* basic_specification_names[]{"(deprecated) none",       //
                                                                 "(deprecated) orthogonal", //
                                                                 "(deprecated) diagonal",   //
                                                                 "spatial",
                                                                 "ro45",
                                                                 "spatial_ro45",
                                                                 "permutation"};
        inline namespace s {
            // TODO: currently in a new namespace to avoid enumerator clash...
            enum extra_specification : int { none = 0, paired, state, size };
        } // namespace s

        static constexpr const char* extra_specification_names[]{"basic", "paired", "state"};

        inline const partitionT& get_partition(basic_specification basic, extra_specification extr) {
            using mapperP = codeT (*)(codeT);

            constexpr auto make_partition = [](const vector<mapperP>& mappers) -> partitionT {
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
                for (codeT code = 0; code < 512; code++) {
                    if (part[code] == -1) {
                        equiv(code, color++, equiv);
                    }
                }
                return part;
            };

            // TODO: very strange format...
#define mapto(...)                                       \
    +[](codeT code) {                                    \
        auto [q, w, e, a, s, d, z, x, c] = decode(code); \
        return encode(__VA_ARGS__);                      \
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
                {},                                                     // none.
                {upside_down, leftside_right},                          // orthogonal
                {main_diag, side_diag},                                 // diagonal
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

// TODO: in development...
namespace legacy {
    // TODO: whether to allow flip mode?
    struct modelT {
        // TODO: it's easy to define an invalid state, but how to adapt with partition?
        enum state : char { _0, _1, unknown };
        std::array<state, 512> data;

        modelT() { reset(); }

        void set(codeT code, bool b) {
            // TODO: explain decision against invalid situ
            if (data[code] == unknown) {
                data[code] = state{b};
            }
        }

        void reset() { data.fill(unknown); }

        auto bind(const legacy::ruleT& rule) {
            // TODO: is this const?
            return [this, rule](codeT code) /*const*/ {
                set(code, rule(code));
                return rule(code);
            };
        }
    };

    inline vector<modelT::state> filter(const modelT& m, const partitionT& p) {
        using enum modelT::state;

        vector<modelT::state> grouped(p.k(), modelT::unknown);

        for (int j = 0; j < p.k(); ++j) {
            bool has_0 = false;
            bool has_1 = false;
            for (codeT code : p.groups()[j]) {
                if (m.data[code] == _0) {
                    has_0 = true;
                } else if (m.data[code] == _1) {
                    has_1 = true;
                }
            }
            if (has_0 && has_1) {
                // TODO: What to do?
            }
            if (has_0) {
                grouped[j] = _0;
            } else if (has_1) {
                grouped[j] = _1;
            }
        }
        return grouped;
    }

    ruleT make_rule(const modelT& m, const partitionT& p, auto&& rand) {
        // step 1:
        using enum modelT::state;

        vector<modelT::state> grouped = filter(m, p);

        // step 2:
        // TODO: doesn't make sense...
        for (auto& s : grouped) {
            if (s == unknown) {
                s = (rand() & 0b111) == 1 ? _1 : _0;
            }
        }

        // step 3: dispatch...
        ruleT rule{};
        for (codeT code = 0; code < 512; ++code) {
            rule.map[code] = grouped[p.map(code)] == _0 ? 0 : 1;
        }
        return rule;
    }
} // namespace legacy
