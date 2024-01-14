#pragma once

#include <charconv>
#include <format> // TODO: utils...
#include <random> // TODO: utils...
#include <span>

#include "rule.hpp"

// TODO: together with tileT functions -> utils header...
// TODO: explain...
inline std::mt19937& global_mt19937() {
    static std::mt19937 rand(time(0));
    return rand;
}

namespace legacy {
    static_assert(INT_MAX >= INT32_MAX);

    // TODO: rename to sizeT? tileT::sizeT & tileT::posT?
    // TODO: for posT/rectT, whether to pass by value or by reference?
    struct rectT {
        int width, height;
        friend bool operator==(const rectT&, const rectT&) = default;
    };

    struct posT {
        int x, y;
        friend bool operator==(const posT&, const posT&) = default;
    };

    inline posT as_pos(const rectT& size) {
        return {.x = size.width, .y = size.height};
    }

    // TODO: explain layout... reorganize for better readability...
    // TODO: add assertions, especially about empty tileT... (e.g. currently even operator== is invalid)
    class tileT {
        rectT m_size; // observable width and height.
        bool* m_data; // layout: [height+2][width]|[height+2][2].

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

        explicit tileT(const rectT& size) : tileT() {
            if (size.width > 0 && size.height > 0) {
                m_size = size;
                m_data = new bool[(m_size.width + 2) * (m_size.height + 2)]{};
            }
        }
        ~tileT() { delete[] m_data; }

        // TODO: rephrase...
        // conceptually write-only after this call...
        void resize(const rectT& size) {
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
        rectT size() const { return m_size; }

        int width() const { return m_size.width; }
        int height() const { return m_size.height; }
        int area() const { return m_size.width * m_size.height; }

    private:
        bool* _line(int _y) {
            assert(_y >= 0 && _y < m_size.height + 2);
            return m_data + _y * m_size.width;
        }
        const bool* _line(int _y) const {
            assert(_y >= 0 && _y < m_size.height + 2);
            return m_data + _y * m_size.width;
        }

    public:
        bool* line(int y) {
            assert(y >= 0 && y < m_size.height);
            return _line(y + 1);
        }
        const bool* line(int y) const {
            assert(y >= 0 && y < m_size.height);
            return _line(y + 1);
        }

        // TODO: line(y,xbeg,xend)?
        // TODO: at(posT)?

        // TODO: ? whether to expose consecutive data?
        std::span<bool> data() { return {line(0), line(0) + area()}; }
        std::span<const bool> data() const { return {line(0), line(0) + area()}; }

    private:
        void _set_lr(int _y, bool l, bool r) {
            assert(_y >= 0 && _y < m_size.height + 2);
            bool* lr = m_data + m_size.width * (m_size.height + 2);
            lr[_y * 2] = l;
            lr[_y * 2 + 1] = r;
        }

        std::pair<bool, bool> _get_lr(int _y) const {
            assert(_y >= 0 && _y < m_size.height + 2);
            const bool* lr = m_data + m_size.width * (m_size.height + 2);
            return {lr[_y * 2], lr[_y * 2 + 1]};
        }

    public:
        // TODO: This could be used to support boundless space.
        // TODO: const or not?
        // (The problem is that, t.gather(t,t,t,t,t,t,t,t) is intentionally a valid operation, so passing by const&
        // seems an over-promise...)
        void gather( // clang-format off
            const tileT& q, const tileT& w, const tileT& e,
            const tileT& a, /*   *this   */ const tileT& d,
            const tileT& z, const tileT& x, const tileT& c
        ) { // clang-format on
            // assert m_shape == *.m_shape.
            const int width = m_size.width, height = m_size.height;

            std::copy_n(w._line(height), width, _line(0));
            std::copy_n(x._line(1), width, _line(height + 1));

            _set_lr(0, q._line(height)[width - 1], e._line(height)[0]);
            _set_lr(height + 1, z._line(1)[width - 1], c._line(1)[0]);
            for (int _y = 1; _y <= height; ++_y) {
                _set_lr(_y, a._line(_y)[width - 1], d._line(_y)[0]);
            }
        }

        // TODO: This could be used to support constraint gathering...
        // TODO: go back to template when needed...
        // I hate this function, it is the payment for consecutive data...
        void apply(const auto& rule, tileT& dest) const {
            // pre: already gathered ???<TODO>, which is untestable.
            assert(this != &dest);
            dest.resize(m_size);

            const int width = m_size.width, height = m_size.height;

            if (width == 1) [[unlikely]] {
                for (int _y = 1; _y <= height; ++_y) {
                    const auto [_q, _e] = _get_lr(_y - 1);
                    const auto [_a, _d] = _get_lr(_y);
                    const auto [_z, _c] = _get_lr(_y + 1);
                    const bool _w = _line(_y - 1)[0];
                    const bool _s = _line(_y)[0];
                    const bool _x = _line(_y + 1)[0];
                    bool* _dest = dest._line(_y);

                    _dest[0] = rule(encode(_q, _w, _e, _a, _s, _d, _z, _x, _c));
                }
                return;
            }

            for (int _y = 1; _y <= height; ++_y) {
                const auto [_q, _e] = _get_lr(_y - 1);
                const auto [_a, _d] = _get_lr(_y);
                const auto [_z, _c] = _get_lr(_y + 1);

                const bool* _up = _line(_y - 1); // _q _up _e
                const bool* _ct = _line(_y);     // _a _ct _d
                const bool* _dw = _line(_y + 1); // _z _dw _c

                bool* _dest = dest._line(_y);

                // clang-format off
                _dest[0] = rule(encode(_q, _up[0], _up[1],
                                       _a, _ct[0], _ct[1],
                                       _z, _dw[0], _dw[1]));
                for (int x = 1; x < width - 1; ++x) {
                    _dest[x] = rule(encode(_up[x - 1], _up[x], _up[x + 1],
                                           _ct[x - 1], _ct[x], _ct[x + 1],
                                           _dw[x - 1], _dw[x], _dw[x + 1]));
                }
                _dest[width - 1] = rule(encode(_up[width - 2], _up[width - 1], _e,
                                               _ct[width - 2], _ct[width - 1], _d,
                                               _dw[width - 2], _dw[width - 1], _c));
                // clang-format on
            }
        }

        friend bool operator==(const tileT& l, const tileT& r) { //
            return l.m_size == r.m_size && std::ranges::equal(l.data(), r.data());
        }
    };

#ifndef NDEBUG
    namespace _misc {
        // TODO: enhance this test?
        inline const bool test_tileT = [] {
            const rectT size{1, 1};
            tileT t_q(size), t_w(size), t_e(size);
            tileT t_a(size), t_s(size), t_d(size);
            tileT t_z(size), t_x(size), t_c(size);

            tileT dest(size);

            const ruleT gol = game_of_life();
            for_each_code(code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                t_q.line(0)[0] = q, t_w.line(0)[0] = w, t_e.line(0)[0] = e;
                t_a.line(0)[0] = a, t_s.line(0)[0] = s, t_d.line(0)[0] = d;
                t_z.line(0)[0] = z, t_x.line(0)[0] = x, t_c.line(0)[0] = c;

                t_s.gather(t_q, t_w, t_e, t_a, t_d, t_z, t_x, t_c);
                t_s.apply(gol, dest);
                assert(dest.line(0)[0] == gol(code));
            }
            return true;
        }();
    } // namespace _misc
#endif

    inline namespace tileT_utils {
        inline int count(const tileT& tile) {
            int c = 0;
            for (bool b : tile.data()) {
                c += b;
            }
            return c;
        }

#if 0
        inline int count_diff(const tileT& l, const tileT& r) {
            assert(l.size() == r.size());
            int c = 0;
            const bool* l_data = l.line(0);
            for (bool b : r.data()) {
                c += (b != *l_data++);
            }
            return c;
        }
#endif

        inline bool verify_pos(const tileT& tile, posT begin, posT end) {
            return 0 <= begin.x && begin.x <= end.x && end.x <= tile.width() && //
                   0 <= begin.y && begin.y <= end.y && end.y <= tile.height();
        }

        // TODO: is this copy or paste???
        enum class copyE { Value, Or, Xor };
        template <copyE mode = copyE::Value>
        inline void copy(const tileT& source, posT begin, posT end, tileT& dest, posT dbegin) {
            assert(&source != &dest);
            assert(verify_pos(source, begin, end));
            // TODO: dbegin...

            const int width = end.x - begin.x, height = end.y - begin.y;
            for (int dy = 0; dy < height; ++dy) {
                const bool* const s = source.line(begin.y + dy) + begin.x;
                bool* const d = dest.line(dbegin.y + dy) + dbegin.x;
                for (int dx = 0; dx < width; ++dx) {
                    if constexpr (mode == copyE::Value) {
                        d[dx] = s[dx];
                    } else if constexpr (mode == copyE::Or) {
                        d[dx] |= s[dx];
                    } else {
                        static_assert(mode == copyE::Xor);
                        d[dx] ^= s[dx];
                    }
                }
            }
        }

        template <copyE mode = copyE::Value>
        inline void copy(const tileT& source, tileT& dest, posT dbegin) {
            copy<mode>(source, {0, 0}, as_pos(source.size()), dest, dbegin);
        }

        // TODO: add param const...
        // TODO: is the algo correct?
        // TODO: explain why not using bernoulli_distribution
        inline void random_fill(tileT& tile, std::mt19937& rand, double density, posT begin, posT end) {
            assert(verify_pos(tile, begin, end));

            const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
            for (int y = begin.y; y < end.y; ++y) {
                bool* line = tile.line(y);
                std::generate(line + begin.x, line + end.x, [&] { return rand() < c; });
            }
        }

        inline void random_fill(tileT& tile, std::mt19937& rand, double density) {
            random_fill(tile, rand, density, {0, 0}, as_pos(tile.size()));
        }

        inline void clear_inside(tileT& tile, posT begin, posT end, bool v) {
            assert(verify_pos(tile, begin, end));

            for (int y = begin.y; y < end.y; ++y) {
                bool* line = tile.line(y);
                std::fill(line + begin.x, line + end.x, v);
            }
        }

        inline void clear_outside(tileT& tile, posT begin, posT end, bool v) {
            assert(verify_pos(tile, begin, end));

            const int width = tile.width(), height = tile.height();
            for (int y = 0; y < height; ++y) {
                bool* line = tile.line(y);
                for (int x = 0; x < width; ++x) {
                    if (y < begin.y || y >= end.y || x < begin.x || x >= end.x) {
                        line[x] = v;
                    }
                }
            }
        }

        void shrink(const tileT& tile, posT& begin, posT& end, bool v);

#if 0
        // TODO: is the name meaningful?
        // Return smallest rectT ~ (>= target) && (% period == 0)
        inline rectT upscale(const rectT& target, const rectT& period) {
            const auto upscale = [](int target, int period) { //
                return ((target + period - 1) / period) * period;
            };

            return {.width = upscale(target.width, period.width), //
                    .height = upscale(target.height, period.height)};
        }

        inline void piece_up(const tileT& period, tileT& target);

        inline void piece_up(const tileT& period, tileT& target, posT begin /*[*/, posT end /*)*/);
#endif
    } // namespace tileT_utils

    // TODO: As to pattern modification:
    // clipboard-based copy/paste is wasteful... enable in-memory mode (tileT-based)...

    // https://conwaylife.com/wiki/Run_Length_Encoded
    inline std::string to_RLE_str(const tileT& tile, posT begin, posT end) {
        assert(verify_pos(tile, begin, end));
        const int width = end.x - begin.x;

        // (wontfix) consecutive '$'s are not combined.
        std::string str;
        int last_nl = 0;
        for (int y = begin.y; y < end.y; ++y) {
            if (y != begin.y) {
                str += '$';
            }

            int c = 0;
            bool v = 0;
            auto flush = [&] {
                if (c != 0) {
                    if (str.size() > last_nl + 60) {
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
            for (const bool b : std::span(tile.line(y) + begin.x, width)) {
                if (v != b) {
                    flush();
                    v = b;
                }
                ++c;
            }
            flush(); // TODO: whether to ignore trailing zeros?
        }
        return str;
    }

    inline std::string to_RLE_str(const tileT& tile) {
        return to_RLE_str(tile, {0, 0}, as_pos(tile.size()));
    }

    inline std::string to_RLE_str(const ruleT& rule, const tileT& tile) {
        return std::format("x = {}, y = {}, rule = {}\n{}!", tile.width(), tile.height(), to_MAP_str(rule),
                           to_RLE_str(tile));
    }

    inline std::string to_RLE_str(const ruleT& rule, const tileT& tile, posT begin, posT end) {
        assert(verify_pos(tile, begin, end));
        return std::format("x = {}, y = {}, rule = {}\n{}!", end.x - begin.x, end.y - begin.y, to_MAP_str(rule),
                           to_RLE_str(tile, begin, end));
    }

    // TODO: parse leading line (x = ...)
    // TODO: what if not matching? currently returning a 1x1 dot... should return nothing...
    inline tileT from_RLE_str(std::string_view text, const rectT max_size) {
        {
            const char *str = text.data(), *end = str + text.size();
            // TODO: temp; skip first line; test only...
            if (*str == 'x') {
                while (str != end && *str != '\n') {
                    ++str;
                }
                ++str;
            }
            text = std::string_view(str, end);
        }

        auto take = [end = text.data() + text.size()](const char*& str) -> std::pair<int, char> {
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
        };

        long long width = 1, height = 1;
        {
            long long x = 0;
            for (const char* str = text.data();;) {
                const auto [n, c] = take(str);
                assert(n >= 0);
                if (c == '!') {
                    break;
                } else if (c == 'b' || c == 'o') {
                    x += n;
                } else {
                    assert(c == '$');
                    height += n;
                    width = std::max(width, x);
                    x = 0;
                }
            }
            width = std::max(width, x);
        }

        // TODO: if width, height==1, return {}?

        // TODO: exceptions like this should be reported via popup?
        if (width > max_size.width || height > max_size.height) {
            throw std::runtime_error(std::format("Size too big: x = {}, y = {}\nLimit: x <= {}, y <= {}", width, height,
                                                 max_size.width, max_size.height));
        }

        tileT tile({.width = (int)width, .height = (int)height});
        int x = 0, y = 0;
        for (const char* str = text.data();;) {
            const auto [n, c] = take(str);
            if (c == '!') {
                break;
            }
            switch (c) {
            case 'b':
            case 'o':
                std::fill_n(tile.line(y) + x, n, c == 'o');
                x += n;
                break;
            case '$':
                y += n;
                x = 0;
                break;
            }
        }
        return tile;
    }

#ifndef NDEBUG
    // TODO: update when from_RLE_str accepts x = ... line...
    namespace _misc {
        inline const bool test_RLE_str = [] {
            tileT tile({.width = 32, .height = 60});
            random_fill(tile, global_mt19937(), 0.5);
            assert(tile == from_RLE_str(to_RLE_str(tile), tile.size()));
            return true;
        }();
    } // namespace _misc
#endif
} // namespace legacy
