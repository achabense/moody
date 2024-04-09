#pragma once

#include <charconv>
#include <format>

#include "rule.hpp"

namespace legacy {
    // Building block for torus space etc.
    class tileT {
    public:
        struct posT {
            int x, y;
            friend bool operator==(const posT&, const posT&) = default;
        };

        struct sizeT {
            int width, height;
            friend bool operator==(const sizeT&, const sizeT&) = default;
        };

        friend posT operator+(const posT& pos, const sizeT& size) {
            return {.x = pos.x + size.width, .y = pos.y + size.height};
        }

        struct rangeT {
            posT begin, end; // [)
            int width() const {
                assert(begin.x <= end.x);
                return end.x - begin.x;
            }
            int height() const {
                assert(begin.y <= end.y);
                return end.y - begin.y;
            }
            sizeT size() const { return {.width = width(), .height = height()}; }
        };

    private:
        sizeT m_size; // Observable width and height.
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

        explicit tileT(sizeT size) : tileT() {
            if (size.width > 0 && size.height > 0) {
                m_size = size;
                m_data = new bool[(m_size.width + 2) * (m_size.height + 2)]{};
            }
        }
        ~tileT() { delete[] m_data; }

        // The line data is conceptually write-only after `resize`.
        void resize(sizeT size) {
            if (m_size != size) {
                tileT resized(size);
                swap(resized);
            }
        }

        tileT(const tileT& other) : tileT(other.m_size) {
            if (other.m_data) {
                std::copy_n(other.m_data, (m_size.width + 2) * (m_size.height + 2), m_data);
            }
        }
        tileT& operator=(const tileT& other) {
            if (this != &other) {
                resize(other.m_size);
                if (other.m_data) {
                    std::copy_n(other.m_data, (m_size.width + 2) * (m_size.height + 2), m_data);
                }
            }
            return *this;
        }

    public:
        sizeT size() const { return m_size; }

        int width() const { return m_size.width; }
        int height() const { return m_size.height; }
        int area() const { return m_size.width * m_size.height; }

        rangeT entire_range() const { return {{0, 0}, {.x = m_size.width, .y = m_size.height}}; }
        bool has_range(const rangeT& range) const {
            const auto& [begin, end] = range;
            return 0 <= begin.x && begin.x <= end.x && end.x <= m_size.width && //
                   0 <= begin.y && begin.y <= end.y && end.y <= m_size.height;
        }

    private:
        bool* _line(int _y) {
            assert(m_data && _y >= 0 && _y < m_size.height + 2);
            return m_data + _y * (m_size.width + 2);
        }
        const bool* _line(int _y) const {
            assert(m_data && _y >= 0 && _y < m_size.height + 2);
            return m_data + _y * (m_size.width + 2);
        }

    public:
        bool* line(int y) {
            assert(y >= 0 && y < m_size.height);
            return _line(y + 1) + 1;
        }
        const bool* line(int y) const {
            assert(y >= 0 && y < m_size.height);
            return _line(y + 1) + 1;
        }

        void for_each_line(const rangeT& range, const auto& fn) { //
            _for_each_line(*this, range, fn);
        }

        void for_each_line(const rangeT& range, const auto& fn) const { //
            _for_each_line(*this, range, fn);
        }

    private:
        static void _for_each_line(auto& this_, const rangeT& range, const auto& fn) {
            assert(this_.has_range(range));
            const auto& [begin, end] = range;
            for (int y = begin.y; y < end.y; ++y) {
                auto line = this_.line(y); // bool* or const bool*
                if constexpr (requires { fn(std::span{line + begin.x, line + end.x}); }) {
                    fn(std::span{line + begin.x, line + end.x});
                } else {
                    fn(y - begin.y, std::span{line + begin.x, line + end.x});
                }
            }
        }

    public:
        // (`q`, `w`, ... may refer to `*this`, see below.)
        void gather(const tileT& q, const tileT& w, const tileT& e, //
                    const tileT& a, /*   *this   */ const tileT& d, //
                    const tileT& z, const tileT& x, const tileT& c) {
            // assert m_size == *.m_size.

            const int width = m_size.width, height = m_size.height;

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

            for (int _y = 1; _y <= m_size.height; ++_y) {
                const bool* _up = _line(_y - 1);
                const bool* _cn = _line(_y);
                const bool* _dw = _line(_y + 1);
                bool* _dest = dest._line(_y);

                for (int _x = 1; _x <= m_size.width; ++_x) {
                    _dest[_x] = rule(encode({
                        _up[_x - 1], _up[_x], _up[_x + 1], //
                        _cn[_x - 1], _cn[_x], _cn[_x + 1], //
                        _dw[_x - 1], _dw[_x], _dw[_x + 1], //
                    }));
                }
            }
        }

        // (For `capture_open`.)
        void collect(const rangeT& range, moldT::lockT& lock) const {
            // There is supposed to be a call to `gather` before calling `record`
            // if `range` is allowed to be adjacent to the boundary.
            assert(has_range(range));

            for (int _y = range.begin.y + 1; _y <= range.end.y; ++_y) {
                const bool* _up = _line(_y - 1);
                const bool* _cn = _line(_y);
                const bool* _dw = _line(_y + 1);

                for (int _x = range.begin.x + 1; _x <= range.end.x; ++_x) {
                    lock[encode({
                        _up[_x - 1], _up[_x], _up[_x + 1], //
                        _cn[_x - 1], _cn[_x], _cn[_x + 1], //
                        _dw[_x - 1], _dw[_x], _dw[_x + 1], //
                    })] = true;
                }
            }
        }

        // This is relying on codeT::bpos_q = 0, bpos_w = 1, ... bpos_c = 8.
        void apply_v2(const rule_like auto& rule, tileT& dest) {
            assert(this != &dest);
            dest.resize(m_size);

            const int width = m_size.width, height = m_size.height;
            std::vector<char> _vec_a(width + 1), _vec_b(width + 1); // [0, width]

            char *_vec_up = _vec_a.data(), *_vec_cn = _vec_b.data();
            {
                const bool *_up = _line(0), *_cn = _line(1);
                int p3_up = (_up[0] << 1) | (_up[1] << 2);
                int p3_cn = (_cn[0] << 1) | (_cn[1] << 2);
                for (int _x = 1; _x <= width; ++_x) {
                    p3_up = (p3_up >> 1) | (_up[_x + 1] << 2);
                    p3_cn = (p3_cn >> 1) | (_cn[_x + 1] << 2);
                    _vec_up[_x] = p3_up; // _up[_x - 1] | (_up[_x] << 1) | (_up[_x + 1] << 2)
                    _vec_cn[_x] = p3_cn; // _cn[_x - 1] | (_cn[_x] << 1) | (_cn[_x + 1] << 2)
                }
            }
            for (int _y = 1; _y <= height; ++_y) {
                const bool* _dw = _line(_y + 1);
                bool* _dest = dest._line(_y);
                int p3_dw = (_dw[0] << 1) | (_dw[1] << 2);
                for (int _x = 1; _x <= width; ++_x) {
                    p3_dw = (p3_dw >> 1) | (_dw[_x + 1] << 2);
                    _dest[_x] = rule(codeT{_vec_up[_x] | (_vec_cn[_x] << 3) | (p3_dw << 6)});
                    _vec_up[_x] = p3_dw; // _dw[_x - 1] | (_dw[_x] << 1) | (_dw[_x + 1] << 2)
                }
                std::swap(_vec_up, _vec_cn);
            }
        }

        friend bool operator==(const tileT& l, const tileT& r) {
            if (l.m_size != r.m_size) {
                return false;
            }
            for (int y = 0; y < l.m_size.height; ++y) {
                const bool* l_line = l.line(y);
                const bool* r_line = r.line(y);
                if (!std::equal(l_line, l_line + l.m_size.width, r_line)) {
                    return false;
                }
            }
            return true;
        }
    };

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_tileT_apply = [] {
            const tileT::sizeT size{1, 1};

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

        inline const testT test_tileT_apply_v2 = [] {
            const tileT::sizeT size{32, 32};
            tileT source(size), dest(size), dest_v2(size);
            source.for_each_line(source.entire_range(), [&](std::span<bool> line) { //
                std::ranges::generate(line, [&] { return testT::rand() & 1; });
            });

            const ruleT rule = make_rule([](codeT) { return testT::rand() & 1; });
            source.gather_torus();
            source.apply(rule, dest);
            source.apply(rule, dest_v2);
            assert(dest == dest_v2);
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    using rangeT_opt = std::optional<tileT::rangeT>;

    inline int count(const tileT& tile, const rangeT_opt& range = {}) {
        int c = 0;
        tile.for_each_line(range.value_or(tile.entire_range()), [&c](std::span<const bool> line) {
            for (bool v : line) {
                c += v;
            }
        });
        return c;
    }

    enum class blitE { Copy, Or, And, Xor };
    template <blitE mode>
    inline void blit(tileT& dest, tileT::posT d_begin, const tileT& source, const rangeT_opt& s_range_ = {}) {
        assert(&source != &dest);

        const tileT::rangeT s_range = s_range_.value_or(source.entire_range());
        assert(dest.has_range({d_begin, d_begin + s_range.size()}));

        source.for_each_line(s_range, [&dest, &d_begin](int y, std::span<const bool> line) {
            bool* d = dest.line(d_begin.y + y) + d_begin.x;
            for (bool v : line) {
                if constexpr (mode == blitE::Copy) {
                    *d++ = v;
                } else if constexpr (mode == blitE::Or) {
                    *d++ |= v;
                } else if constexpr (mode == blitE::And) {
                    *d++ &= v;
                } else {
                    static_assert(mode == blitE::Xor);
                    *d++ ^= v;
                }
            }
        });
    }

    inline tileT copy(const tileT& source, const rangeT_opt& range_ = {}) {
        const tileT::rangeT range = range_.value_or(source.entire_range());

        tileT tile(range.size());
        blit<blitE::Copy>(tile, {0, 0}, source, range);
        return tile;
    }

    // The result will be completely dependent on the state of `rand` and `density`; `bernoulli_distribution`
    // cannot guarantee this.
    inline void random_fill(tileT& tile, std::mt19937& rand, double density, const rangeT_opt& range = {}) {
        const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
        tile.for_each_line(range.value_or(tile.entire_range()), [&](std::span<bool> line) { //
            std::ranges::generate(line, [&] { return rand() < c; });
        });
    }

    inline void clear_inside(tileT& tile, const tileT::rangeT& range /* Required */, const bool v = 0) {
        tile.for_each_line(range, [v](std::span<bool> line) { std::ranges::fill(line, v); });
    }

    inline void clear_outside(tileT& tile, const tileT::rangeT& range /* Required */, const bool v = 0) {
        assert(tile.has_range(range));
        tile.for_each_line(tile.entire_range(), [&](int y, std::span<bool> line) {
            if (y < range.begin.y || y >= range.end.y) {
                std::ranges::fill(line, v);
            } else {
                std::fill(line.begin(), line.begin() + range.begin.x, v);
                std::fill(line.begin() + range.end.x, line.end(), v);
            }
        });
    }

    inline tileT::rangeT bounding_box(const tileT& tile, const tileT::rangeT& range /* Required */, const bool v = 0) {
        // About the usage of std::string:
        // 1. I hate std::vector<bool>.
        // 2. There is no std::find_last in C++20. (https://en.cppreference.com/w/cpp/algorithm/ranges/find_last)
        std::string has_nv_x(range.width(), false);
        std::string has_nv_y(range.height(), false);

        tile.for_each_line(range, [&](int y, std::span<const bool> line) {
            for (int x = 0; const bool b : line) {
                if (b != v) {
                    has_nv_x[x] = true;
                    has_nv_y[y] = true;
                }
                ++x;
            }
        });

        const auto first_nv_x = has_nv_x.find_first_of(true);
        const auto npos = std::string::npos;
        if (first_nv_x != npos) {
            const auto first_nv_y = has_nv_y.find_first_of(true);
            const auto last_nv_x = has_nv_x.find_last_of(true);
            const auto last_nv_y = has_nv_y.find_last_of(true);
            assert(first_nv_y != npos && last_nv_x != npos && last_nv_y != npos);
            return {.begin{.x = int(range.begin.x + first_nv_x), .y = int(range.begin.y + first_nv_y)},
                    .end{.x = int(range.begin.x + last_nv_x + 1), .y = int(range.begin.y + last_nv_y + 1)}};
        } else {
            return {};
        }
    }

    namespace _misc {
        //  https://conwaylife.com/wiki/Run_Length_Encoded
        // (The "wontfix" things are allowed by the specification.)
        inline void to_RLE(std::string& str, const tileT& tile, const tileT::rangeT& range) {
            // (wontfix) Consecutive '$'s are not combined.
            size_t last_nl = str.size();
            tile.for_each_line(range, [&str, &last_nl](int y, std::span<const bool> line) {
                if (y != 0) {
                    str += '$';
                }

                int c = 0;
                bool v = 0;
                auto flush = [&] {
                    if (c != 0) {
                        // (58 is an arbitrary value that satisfies the line-length limit.)
                        if (str.size() > last_nl + 58) {
                            str += '\n';
                            last_nl = str.size();
                        }

                        if (c != 1) {
                            str += std::to_string(c);
                        }
                        str += "bo"[v];
                        c = 0;
                    }
                };
                for (const bool b : line) {
                    if (v != b) {
                        flush();
                        v = b;
                    }
                    ++c;
                }
                flush(); // (wontfix) Trailing 0s are not omitted.
            });
        }
    } // namespace _misc

    inline std::string to_RLE_str(const ruleT* rule, const tileT& tile, const rangeT_opt& range_ = {}) {
        const tileT::rangeT range = range_.value_or(tile.entire_range());
        std::string str =
            rule ? std::format("x = {}, y = {}, rule = {}\n", range.width(), range.height(), to_MAP_str(*rule))
                 : std::format("x = {}, y = {}\n", range.width(), range.height());
        _misc::to_RLE(str, tile, range);
        str += '!';
        return str;
    }

    // TODO: whether to deal with the header line? ("x = ..., y = ...(, rule = ...)")
    // (This does not matter as long as the patterns are generated by `to_RLE_str`.)
    inline tileT from_RLE_str(std::string_view text, const tileT::sizeT max_size) {
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

        if (width == 0 || height == 0) {
            throw std::runtime_error("Found nothing.");
        }
        if (width > max_size.width || height > max_size.height) {
            throw std::runtime_error(std::format("Size too large: x = {}, y = {}\nLimit: x <= {}, y <= {}", width,
                                                 height, max_size.width, max_size.height));
        }

        tileT tile({.width = (int)width, .height = (int)height});
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
        return tile;
    }

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_RLE_str = [] {
            using listT = std::initializer_list<tileT::sizeT>;
            for (const auto size : listT{{.width = 1, .height = 1},
                                         {.width = 3, .height = 1},
                                         {.width = 1, .height = 3},
                                         {.width = 32, .height = 60}}) {
                tileT tile(size);
                random_fill(tile, testT::rand, 0.5);
                assert(tile == from_RLE_str(to_RLE_str(nullptr, tile), tile.size()));
            }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

} // namespace legacy
