#pragma once

#include <charconv>
#include <format>

#include "rule.hpp"
#include "tile_base.hpp"

namespace aniso {
    // Building block for torus space etc.
    class tileT {
        vecT m_size;  // Observable width and height.
        bool* m_data; // Layout: [height+2][width+2].

    public:
        void swap(tileT& other) noexcept {
            std::swap(m_size, other.m_size);
            std::swap(m_data, other.m_data);
        }
        tileT() noexcept : m_size{0, 0}, m_data{nullptr} {}
        tileT(tileT&& other) noexcept : tileT() { swap(other); }
        tileT& operator=(tileT&& other) noexcept {
            swap(other);
            return *this;
        }

        explicit tileT(vecT size) : tileT() {
            if (size.x > 0 && size.y > 0) {
                m_size = size;
                m_data = new bool[(m_size.x + 2) * (m_size.y + 2)]{};
            }
        }
        ~tileT() { delete[] m_data; }

        // The line data is conceptually write-only after `resize`.
        void resize(vecT size) {
            if (m_size != size) {
                tileT resized(size);
                swap(resized);
            }
        }

        tileT(const tileT& other) : tileT(other.m_size) {
            if (other.m_data) {
                std::copy_n(other.m_data, (m_size.x + 2) * (m_size.y + 2), m_data);
            }
        }
        tileT& operator=(const tileT& other) {
            if (this != &other) {
                resize(other.m_size);
                if (other.m_data) {
                    std::copy_n(other.m_data, (m_size.x + 2) * (m_size.y + 2), m_data);
                }
            }
            return *this;
        }

        vecT size() const { return m_size; }
        int width() const { return m_size.x; }
        int height() const { return m_size.y; }
        int area() const { return m_size.x * m_size.y; }

    private:
        bool* _line(int _y) {
            assert(m_data && _y >= 0 && _y < m_size.y + 2);
            return m_data + _y * (m_size.x + 2);
        }
        const bool* _line(int _y) const {
            assert(m_data && _y >= 0 && _y < m_size.y + 2);
            return m_data + _y * (m_size.x + 2);
        }

    public:
        bool* line(int y) {
            assert(y >= 0 && y < m_size.y);
            return _line(y + 1) + 1;
        }
        const bool* line(int y) const {
            assert(y >= 0 && y < m_size.y);
            return _line(y + 1) + 1;
        }

        tile_ref data() { return {.size = m_size, .stride = m_size.x + 2, .data = line(0)}; }
        tile_const_ref data() const { return {.size = m_size, .stride = m_size.x + 2, .data = line(0)}; }

    public:
        // (`q`, `w`, ... may refer to `*this`, see below.)
        void gather(const tileT& q, const tileT& w, const tileT& e, //
                    const tileT& a, /*   *this   */ const tileT& d, //
                    const tileT& z, const tileT& x, const tileT& c) {
            // assert m_size == *.m_size.

            const int width = m_size.x, height = m_size.y;

            auto _set_lr = [width](bool* _line, bool l, bool r) {
                _line[0] = l;
                _line[width + 1] = r;
            };

            _set_lr(_line(0), q._line(height)[width], e._line(height)[1]);
            std::copy_n(w._line(height) + 1, width, _line(0) + 1);

            for (int _y = 1; _y <= height; ++_y) {
                _set_lr(_line(_y), a._line(_y)[width], d._line(_y)[1]);
            }

            _set_lr(_line(height + 1), z._line(1)[width], c._line(1)[1]);
            std::copy_n(x._line(1) + 1, width, _line(height + 1) + 1);
        }

        void gather_torus() { gather(*this, *this, *this, *this, *this, *this, *this, *this); }

        void apply(const rule_like auto& rule, tileT& dest) const {
            // There is supposed to be a call to `gather` before calling `apply`.
            // (Which is untestable and must be guaranteed by the callers.)

            assert(this != &dest);
            dest.resize(m_size);

            for (int _y = 1; _y <= m_size.y; ++_y) {
                const bool* _up = _line(_y - 1);
                const bool* _cn = _line(_y);
                const bool* _dw = _line(_y + 1);
                bool* _dest = dest._line(_y);

                for (int _x = 1; _x <= m_size.x; ++_x) {
                    _dest[_x] = rule(encode({
                        _up[_x - 1], _up[_x], _up[_x + 1], //
                        _cn[_x - 1], _cn[_x], _cn[_x + 1], //
                        _dw[_x - 1], _dw[_x], _dw[_x + 1], //
                    }));
                }
            }
        }

        // This is relying on codeT::bpos_q = 0, bpos_w = 1, ... bpos_c = 8.
        void apply_v3(const rule_like auto& rule, tileT& dest) const {
            if (&dest != this) {
                dest.resize(m_size);
            }

            const int width = m_size.x, height = m_size.y;
            std::vector<char> _vec(width + 1); // [0, width]

            char* const _vec_p6 = _vec.data();
            {
                const bool *_up = _line(0), *_cn = _line(1);
                int p3_up = (_up[0] << 1) | (_up[1] << 2);
                int p3_cn = (_cn[0] << 1) | (_cn[1] << 2);
                for (int _x = 1; _x <= width; ++_x) {
                    p3_up = (p3_up >> 1) | (_up[_x + 1] << 2);
                    p3_cn = (p3_cn >> 1) | (_cn[_x + 1] << 2);
                    _vec_p6[_x] = p3_up | (p3_cn << 3);
                    // _vec_p6[_x] =
                    // (_up[_x - 1] << 0) | (_up[_x] << 1) | (_up[_x + 1] << 2)
                    // (_cn[_x - 1] << 3) | (_cn[_x] << 4) | (_cn[_x + 1] << 5)
                }
            }
            for (int _y = 1; _y <= height; ++_y) {
                const bool* _dw = _line(_y + 1);
                bool* _dest = dest._line(_y);
                int p3_dw = (_dw[0] << 1) | (_dw[1] << 2);
                for (int _x = 1; _x <= width; ++_x) {
                    p3_dw = (p3_dw >> 1) | (_dw[_x + 1] << 2);
                    // p3_dw = _dw[_x - 1] | (_dw[_x] << 1) | (_dw[_x + 1] << 2)
                    const int code = _vec_p6[_x] | (p3_dw << 6);
                    _dest[_x] = rule(codeT{code});
                    _vec_p6[_x] = code >> 3;
                }
            }
        }

        void apply_v3(const rule_like auto& rule) { apply_v3(rule, *this); }

        friend bool operator==(const tileT& l, const tileT& r) {
            if (l.m_size != r.m_size) {
                return false;
            }
            for (int y = 0; y < l.m_size.y; ++y) {
                const bool* l_line = l.line(y);
                const bool* r_line = r.line(y);
                if (!std::equal(l_line, l_line + l.m_size.x, r_line)) {
                    return false;
                }
            }
            return true;
        }
    };

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_tileT_apply = [] {
            const vecT size{1, 1};

            tileT t_q(size), t_w(size), t_e(size);
            tileT t_a(size), t_s(size), t_d(size);
            tileT t_z(size), t_x(size), t_c(size);
            tileT dest(size);

            const ruleT rule = make_rule([](codeT) { return testT::rand() & 1; });
            for_each_code([&](codeT code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                t_q.line(0)[0] = q, t_w.line(0)[0] = w, t_e.line(0)[0] = e;
                t_a.line(0)[0] = a, t_s.line(0)[0] = s, t_d.line(0)[0] = d;
                t_z.line(0)[0] = z, t_x.line(0)[0] = x, t_c.line(0)[0] = c;

                t_s.gather(t_q, t_w, t_e, t_a, t_d, t_z, t_x, t_c);
                t_s.apply(rule, dest);
                assert(dest.line(0)[0] == rule(code));
            });
        };

        inline const testT test_tileT_apply_v3 = [] {
            const vecT size{32, 32};
            tileT source(size), dest(size), dest_v3(size);
            source.data().for_all_data([&](std::span<bool> line) { //
                std::ranges::generate(line, [&] { return testT::rand() & 1; });
            });

            const ruleT rule = make_rule([](codeT) { return testT::rand() & 1; });
            source.gather_torus();

            source.apply(rule, dest);
            source.apply_v3(rule, dest_v3);
            assert(dest == dest_v3);

            source.apply_v3(rule);
            assert(dest == source);
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    // (For `capture_open`.)
    inline void collect_cases(const tile_const_ref tile, moldT::lockT& lock) {
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

    enum class blitE { Copy, Or, And, Xor };
    template <blitE mode>
    inline void blit(const tile_ref dest, const tile_const_ref source) {
        // (`dest` and `source` should not overlap.)

        dest.for_all_data_vs(source, [](bool* d, const bool* s, int w) {
            for (int x = 0; x < w; ++x) {
                if constexpr (mode == blitE::Copy) {
                    d[x] = s[x];
                } else if constexpr (mode == blitE::Or) {
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

    inline tileT copy(const tile_const_ref source) {
        tileT tile(source.size);
        blit<blitE::Copy>(tile.data(), source);
        return tile;
    }

    // The result will be completely dependent on the state of `rand` and `density`; `bernoulli_distribution`
    // cannot guarantee this.
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

    // Empty -> failure; the reason is not cared about.
    inline tileT from_RLE_str_silent(std::string_view text, const vecT max_size) {
        tileT tile{};
        from_RLE_str(text, [&](long long w, long long h) -> std::optional<tile_ref> {
            if (w == 0 || h == 0 || w > max_size.x || h > max_size.y) {
                return std::nullopt;
            } else {
                tile.resize({.x = int(w), .y = int(h)});
                return tile.data();
            }
        });
        return tile;
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_RLE_str = [] {
            using listT = std::initializer_list<vecT>;
            for (const auto size : listT{{.x = 1, .y = 1}, {.x = 3, .y = 1}, {.x = 1, .y = 3}, {.x = 32, .y = 60}}) {
                tileT tile(size);
                random_fill(tile.data(), testT::rand, 0.5);
                // It's ok to compare directly, as `...).tile` will be empty if not successful.
                assert(tile == from_RLE_str_silent(to_RLE_str(tile.data(), nullptr), tile.size()));
            }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

} // namespace aniso
