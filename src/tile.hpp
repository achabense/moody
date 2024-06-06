#pragma once

#include <charconv>
#include <format>

#include "rule.hpp"
#include "tile_base.hpp"

namespace aniso {
    inline int count(const tile_const_ref tile) {
        int c = 0;
        tile.for_all_data([&c](std::span<const bool> line) {
            for (bool v : line) {
                c += v;
            }
        });
        return c;
    }

    inline int count_diff(const tile_const_ref a, const tile_const_ref b) {
        int c = 0;
        a.for_all_data_vs(b, [&c](const bool* a, const bool* b, int w) {
            for (int x = 0; x < w; ++x) {
                c += a[x] != b[x];
            }
        });
        return c;
    }

    // (`dest` and `source` should not overlap.)
    inline void copy(const tile_ref dest, const tile_const_ref source) {
        dest.for_all_data_vs(source, [](bool* d, const bool* s, int len) { //
            std::copy_n(s, len, d);
        });
    }

    // (`dest` and `source` should not overlap.)
    enum class blitE { Copy, Or, And, Xor };
    template <blitE mode>
    inline void blit(const tile_ref dest, const tile_const_ref source) {
        if constexpr (mode == blitE::Copy) {
            copy(dest, source);
        } else {
            dest.for_all_data_vs(source, [](bool* d, const bool* s, int w) {
                for (int x = 0; x < w; ++x) {
                    if constexpr (mode == blitE::Or) {
                        d[x] |= s[x];
                    } else if constexpr (mode == blitE::And) {
                        d[x] &= s[x];
                    } else {
                        static_assert(mode == blitE::Xor);
                        d[x] ^= s[x];
                    }
                }
            });
        }
    }

    // The result will be completely dependent on the state of `rand` and `density`.
    // (`bernoulli_distribution` cannot guarantee this.)
    inline void random_fill(const tile_ref tile, std::mt19937& rand, double density) {
        const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
        tile.for_all_data([&](std::span<bool> line) { //
            std::ranges::generate(line, [&] { return rand() < c; });
        });
    }

    inline void clear(const tile_ref tile, const bool v = 0) {
        tile.for_all_data([v](std::span<bool> line) { //
            std::ranges::fill(line, v);
        });
    }

    inline void clear_outside(const tile_ref tile, const rangeT& range, const bool v = 0) {
        assert(tile.has_range(range));
        tile.for_each_line([&](int y, std::span<bool> line) {
            if (y < range.begin.y || y >= range.end.y) {
                std::ranges::fill(line, v);
            } else {
                std::fill(line.begin(), line.begin() + range.begin.x, v);
                std::fill(line.begin() + range.end.x, line.end(), v);
            }
        });
    }

    inline rangeT bounding_box(const tile_const_ref tile, const bool v = 0) {
        int min_x = INT_MAX, max_x = INT_MIN;
        int min_y = INT_MAX, max_y = INT_MIN;
        tile.for_each_line([&](int y, std::span<const bool> line) {
            for (int x = 0; const bool b : line) {
                if (b != v) {
                    min_x = std::min(min_x, x);
                    max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y);
                    max_y = std::max(max_y, y);
                }
                ++x;
            }
        });
        if (max_x != INT_MIN) {
            return {.begin{.x = min_x, .y = min_y}, .end{.x = max_x + 1, .y = max_y + 1}};
        } else {
            return {};
        }
    }

    inline rangeT bounding_box(const tile_const_ref tile, const rangeT& range, const bool v = 0) {
        assert(tile.has_range(range));
        const rangeT box = bounding_box(tile.clip(range), v);
        if (!box.empty()) {
            return {.begin = range.begin + box.begin, .end = range.begin + box.end};
        } else {
            return {};
        }
    }

    // (`dest` and `source` should not overlap.)
    // Map (0, 0) to (wrap(d.x), wrap(d.y)).
    inline void rotate_copy(const tile_ref dest, const tile_const_ref source, vecT d) {
        assert(dest.size == source.size);
        const vecT size = dest.size;

        const auto wrap = [](int v, int r) { return ((v % r) + r) % r; };
        d.x = wrap(d.x, size.x);
        d.y = wrap(d.y, size.y);
        assert(d.both_gteq({0, 0}) && d.both_lt(size));

        if (d.x == 0 && d.y == 0) [[unlikely]] {
            copy(dest, source);
            return;
        }

#ifndef NDEBUG
        const bool test = source.at({0, 0});
#endif // !NDEBUG
        source.for_each_line([&](int y, std::span<const bool> line) {
            bool* const dest_ = dest.line((y + d.y) % size.y);
            std::copy_n(line.data(), size.x - d.x, dest_ + d.x);
            std::copy_n(line.data() + size.x - d.x, d.x, dest_);
        });
        assert(test == dest.at(d));
    }

    namespace _misc {
        //  https://conwaylife.com/wiki/Run_Length_Encoded
        inline void to_RLE(std::string& str, const tile_const_ref tile) {
            class putT {
                std::string& str;
                size_t last_nl;
                int n = 0;
                char ch = 'b'; // 'b', 'o', '$'.
            public:
                putT(std::string& str) : str(str), last_nl(str.size()) { assert(str.empty() || str.back() == '\n'); }
                void flush() {
                    if (n != 0) {
                        // (58 is an arbitrary value that satisfies the line-length limit.)
                        if (str.size() > last_nl + 58) {
                            str += '\n';
                            last_nl = str.size();
                        }

                        if (n != 1) {
                            str += std::to_string(n);
                        }
                        str += ch;
                        n = 0;
                    }
                }
                void append(int n2, char ch2) {
                    assert(ch2 == 'b' || ch2 == 'o' || ch2 == '$');
                    if (ch == ch2) {
                        n += n2;
                    } else {
                        flush();
                        n = n2;
                        ch = ch2;
                    }
                }
            };

            putT put{str};
            tile.for_each_line([&put, h = tile.size.y](int y, std::span<const bool> line) {
                if (y != 0) {
                    put.append(1, '$');
                }
                const bool *begin = line.data(), *const end = line.data() + line.size();
                while (begin != end) {
                    const bool b = *begin++;
                    int n = 1;
                    while (begin != end && *begin == b) {
                        ++n;
                        ++begin;
                    }
                    const bool omit = y != 0 && y != h - 1 && begin == end && b == 0;
                    if (!omit) {
                        put.append(n, b ? 'o' : 'b');
                    }
                }
            });
            put.flush();
        }
    } // namespace _misc

    inline std::string to_RLE_str(const tile_const_ref tile, const ruleT* rule = nullptr) {
        std::string str = rule ? std::format("x = {}, y = {}, rule = {}\n", tile.size.x, tile.size.y, to_MAP_str(*rule))
                               : std::format("x = {}, y = {}\n", tile.size.x, tile.size.y);
        _misc::to_RLE(str, tile);
        str += '!';
        return str;
    }

    // It's unfortunate that the project didn't consistently use intX_t from the beginning...
    static_assert(INT_MAX < LLONG_MAX);

    // TODO: whether to deal with the header line? ("x = ..., y = ...(, rule = ...)")
    // (This does not matter as long as the patterns are generated by `to_RLE_str`.)
    inline void from_RLE_str(std::string_view text, const auto& prepare) {
        static_assert(requires {
            { prepare((long long)(0), (long long)(0)) } -> std::same_as<std::optional<tile_ref>>;
        });

        if (text.starts_with('x')) {
            text.remove_prefix(std::min(text.size(), text.find('\n')));
        }

        struct takerT {
            const char *str, *end;
            takerT(std::string_view text) : str(text.data()), end(str + text.size()) {}

            std::pair<int, char> take() {
                while (str != end && (*str == '\n' || *str == '\r' || *str == ' ')) {
                    ++str;
                }
                if (str == end) {
                    return {1, '!'};
                }

                int n = 1;
                if (*str >= '1' && *str <= '9') {
                    const auto [ptr, ec] = std::from_chars(str, end, n);
                    if (ec != std::errc{} || ptr == end) {
                        return {1, '!'};
                    }
                    str = ptr;
                }
                assert(str != end);
                switch (*str++) {
                    case 'b': return {n, 'b'};
                    case 'o': return {n, 'o'};
                    case '$': return {n, '$'};
                    default: return {1, '!'};
                }
            }
        };

        const auto [width, height] = [text] {
            long long x = 0, y = 0, xmax = 0;
            for (takerT taker(text);;) {
                const auto [n, c] = taker.take();
                assert(n >= 0);
                if (c == '!') {
                    break;
                } else if (c == 'b' || c == 'o') {
                    x += n;
                    xmax = std::max(xmax, x);
                } else {
                    assert(c == '$');
                    y += n;
                    x = 0;
                }
            }
            return std::pair{xmax, x == 0 ? y : y + 1};
        }();

        if (std::optional<tile_ref> tile_opt = prepare(width, height)) {
            assert(width != 0 && height != 0);
            const tile_ref tile = *tile_opt;
            assert(tile.size.x == width && tile.size.y == height);
            clear(tile, 0); // `tile` data might be dirty.

            int x = 0, y = 0;
            for (takerT taker(text);;) {
                const auto [n, c] = taker.take();
                if (c == '!') {
                    break;
                } else if (c == 'b' || c == 'o') {
                    std::fill_n(tile.line(y) + x, n, c == 'o');
                    x += n;
                } else {
                    y += n;
                    x = 0;
                }
            }
        }
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_RLE_str = [] {
            const vecT sizes[]{{.x = 1, .y = 1}, {.x = 10, .y = 1}, {.x = 1, .y = 10}, {.x = 32, .y = 60}};
            for (const vecT size : sizes) {
                std::unique_ptr<bool[]> a_data(new bool[size.xy()] /* uninitialized */);
                std::unique_ptr<bool[]> b_data(new bool[size.xy()] /* uninitialized */);
                const tile_ref a{size, size.x, a_data.get()};
                const tile_ref b{size, size.x, b_data.get()};
                random_fill(a, testT::rand, 0.5);
                from_RLE_str(to_RLE_str(a, nullptr), [&](long long w, long long h) {
                    assert(w == size.x && h == size.y);
                    return std::optional{b};
                });
                assert(std::equal(a_data.get(), a_data.get() + size.xy(), b_data.get()));
            }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    inline constexpr int calc_border_size(const vecT size) { return 2 * (size.x + size.y) + 4; }

    namespace _misc {
        template <bool is_const>
        struct border_ref_ {
            using boolT = std::conditional_t<is_const, const bool, bool>;

            vecT size;   // For this size.
            boolT* data; // [x][2]*(y+2)[x]

            operator border_ref_<true>() const
                requires(!is_const)
            {
                return {size, data};
            }

            boolT* up_line() const { return data; }
            boolT* down_line() const { return data + size.x + 2 * size.y + 4; }

            void set_lr(int y, bool l, bool r) const
                requires(!is_const)
            {
                assert(y >= -1 && y <= size.y);
                bool* dest = data + size.x + 2 * (y + 1);
                dest[0] = l, dest[1] = r;
            }

            std::pair<bool, bool> get_lr(int y) const {
                assert(y >= -1 && y <= size.y);
                const bool* dest = data + size.x + 2 * (y + 1);
                return {dest[0], dest[1]};
            }

            void collect_from(const tile_const_ref q, const tile_const_ref w, const tile_const_ref e, //
                              const tile_const_ref a, /*        s          */ const tile_const_ref d, //
                              const tile_const_ref z, const tile_const_ref x, const tile_const_ref c) const
                requires(!is_const)
            {
                std::copy_n(w.line(w.size.y - 1), size.x, up_line());
                std::copy_n(x.line(0), size.x, down_line());
                set_lr(-1, q.at({q.size.x - 1, q.size.y - 1}), e.at({0, e.size.y - 1}));
                set_lr(size.y, z.at({z.size.x - 1, 0}), c.at({0, 0}));
                for (int y = 0; y < size.y; ++y) {
                    set_lr(y, a.at({a.size.x - 1, y}), d.at({0, y}));
                }
            }
        };

        // Workaround to avoid creating multiple static variables in `apply_rule` and `apply_rule_torus` (as
        // they are templates). Ideally they should be invisible from anywhere else in the library.
        inline static std::vector<char> helper_memory_for_apply{};

        inline border_ref_<false /* !is_const */> temp_border_for(const vecT size) {
            static std::unique_ptr<bool[]> data{};
            static int len{};

            if (const int required = calc_border_size(size); len < required) {
                data.reset(new bool[required * 2]{});
                len = required * 2;
            }

            return {.size = size, .data = data.get()};
        }

    } // namespace _misc

    using border_ref = _misc::border_ref_<false /* !is_const */>;
    using border_const_ref = _misc::border_ref_<true /* is_const */>;

    // `dest` and `source` may either refer to the same area, or completely non-overlapping.
    inline void apply_rule(const rule_like auto& rule, const tile_ref dest, const tile_const_ref source,
                           const border_const_ref source_border) {
        assert(source.size == dest.size);
        assert(source.size == source_border.size);
        const vecT size = source.size;

        if (_misc::helper_memory_for_apply.size() < size.x) {
            _misc::helper_memory_for_apply.resize(size.x * 2);
        }

        char* const vec_p6 = _misc::helper_memory_for_apply.data();
        {
            const bool *up = source_border.up_line(), *cn = source.line(0);
            const auto [up_l, up_r] = source_border.get_lr(-1);
            const auto [cn_l, cn_r] = source_border.get_lr(0);
            int p3_up = (up_l << 1) | (up[0] << 2);
            int p3_cn = (cn_l << 1) | (cn[0] << 2);
            for (int x = 0; x < size.x - 1; ++x) {
                p3_up = (p3_up >> 1) | (up[x + 1] << 2); // up[x - 1] | (up[x] << 1) | (up[x + 1] << 2)
                p3_cn = (p3_cn >> 1) | (cn[x + 1] << 2); // cn[x - 1] | (cn[x] << 1) | (cn[x + 1] << 2)
                vec_p6[x] = p3_up | (p3_cn << 3);
            }
            p3_up = (p3_up >> 1) | (up_r << 2);
            p3_cn = (p3_cn >> 1) | (cn_r << 2);
            vec_p6[size.x - 1] = p3_up | p3_cn << 3;
        }

        for (int y = 0; y < size.y; ++y) {
            bool* dest_ = dest.line(y);
            const bool* dw = y == size.y - 1 ? source_border.down_line() : source.line(y + 1);
            const auto [dw_l, dw_r] = source_border.get_lr(y + 1);
            int p3_dw = (dw_l << 1) | (dw[0] << 2);
            for (int x = 0; x < size.x - 1; ++x) {
                p3_dw = (p3_dw >> 1) | (dw[x + 1] << 2); // dw[x - 1] | (dw[x] << 1) | (dw[x + 1] << 2)
                const int code = vec_p6[x] | (p3_dw << 6);
                dest_[x] = rule(codeT{code});
                vec_p6[x] = code >> 3;
            }
            p3_dw = (p3_dw >> 1) | (dw_r << 2);
            const int code = vec_p6[size.x - 1] | p3_dw << 6;
            dest_[size.x - 1] = rule(codeT{code});
            vec_p6[size.x - 1] = code >> 3;
        }
    }

    inline void apply_rule_torus(const rule_like auto& rule, const tile_ref dest, const tile_const_ref source) {
        assert(source.size == dest.size);
        const vecT size = source.size;

        const border_ref border = _misc::temp_border_for(size);
        border.collect_from(source, source, source, source, source, source, source, source);
        apply_rule(rule, dest, source, border);
    }

    // (For `capture_open`.)
    inline void fake_apply(const tile_const_ref tile, moldT::lockT& lock) {
        if (tile.size.x <= 2 || tile.size.y <= 2) {
            return;
        }

        for (int y = 1; y < tile.size.y - 1; ++y) {
            const bool* const up = tile.line(y - 1);
            const bool* const cn = tile.line(y);
            const bool* const dw = tile.line(y + 1);
            for (int x = 1; x < tile.size.x - 1; ++x) {
                lock[encode({
                    up[x - 1], up[x], up[x + 1], //
                    cn[x - 1], cn[x], cn[x + 1], //
                    dw[x - 1], dw[x], dw[x + 1], //
                })] = true;
            }
        }
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_apply_1 = [] {
            const ruleT rule = make_rule([](codeT) { return testT::rand() & 1; });

            for_each_code([&](codeT code) {
                const auto make_ref = [](bool& b) { return tile_ref{{1, 1}, 1, &b}; };
                auto [q, w, e, a, s, d, z, x, c] = decode(code);

                bool dest{}, border_data[calc_border_size({1, 1})]{};
                const border_ref border{.size = {1, 1}, .data = border_data};
                border.collect_from(make_ref(q), make_ref(w), make_ref(e), make_ref(a), make_ref(d), make_ref(z),
                                    make_ref(x), make_ref(c));

                apply_rule(rule, make_ref(dest), make_ref(s), border);
                assert(dest == rule[code]);
            });
        };

        inline const testT test_apply_2 = [] {
            const ruleT copy_q = make_rule([](codeT c) { return c.get(codeT::bpos_q); });

            std::array<bool, 120> data[2]{};
            const tile_ref tile{.size{.x = 10, .y = 12}, .stride = 10, .data = &data[0][0]};
            const tile_ref compare{.size{.x = 10, .y = 12}, .stride = 10, .data = &data[1][0]};
            random_fill(tile, testT::rand, 0.5);

            const border_ref border = _misc::temp_border_for({.x = 10, .y = 12});
            for (int i = 0; i < 12; ++i) {
                rotate_copy(compare, tile, {1, 1});
                apply_rule_torus(copy_q, tile, tile);
                assert(data[0] == data[1]);
            }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    struct tileT {
        vecT m_size;
        bool* m_data; // [x]*y

        bool empty() const {
            assert(m_size.x >= 0);
            assert_implies(m_size.x > 0, m_size.y > 0 && m_data);
            return m_size.x == 0;
        }

    public:
        tileT() noexcept : m_size{}, m_data{} {}

        void swap(tileT& other) noexcept {
            std::swap(m_size, other.m_size);
            std::swap(m_data, other.m_data);
        }
        tileT(tileT&& other) noexcept : tileT() { swap(other); }
        tileT& operator=(tileT&& other) noexcept {
            swap(other);
            return *this;
        }

        explicit tileT(const vecT size) : m_size{size}, m_data{} {
            assert(size.both_gteq({0, 0}));
            if (m_size.x > 0) {
                assert(m_size.y > 0);
                m_data = new bool[m_size.xy()]{};
            }
        }

        ~tileT() { delete[] m_data; }

        explicit tileT(const tile_const_ref tile) : tileT(tile.size) {
            assert(!empty());
            copy(data(), tile);
        }

        tileT(const tileT& other) : m_size(other.m_size) {
            if (!other.empty()) {
                m_data = new bool[m_size.xy()];
                std::copy_n(other.m_data, m_size.xy(), m_data);
            }
        }

        void resize(const vecT size) {
            if (m_size != size) {
                tileT(size).swap(*this);
            }
        }

        vecT size() const { return m_size; }

        tile_ref data() {
            assert(!empty());
            return {.size = m_size, .stride = m_size.x /* continuous */, .data = m_data};
        }
        tile_const_ref data() const {
            assert(!empty());
            return {.size = m_size, .stride = m_size.x /* continuous */, .data = m_data};
        }

        void run_torus(const rule_like auto& rule) { //
            apply_rule_torus(rule, data(), data());
        }
    };

} // namespace aniso
