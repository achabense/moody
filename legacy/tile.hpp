#pragma once

#include <charconv>
#include <span>

#include "rule.hpp"

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

        // TODO: is this copy or paste???
        enum class copyE { Value, Or, Xor };
        template <copyE mode = copyE::Value>
        inline void copy(const tileT& source, posT begin, posT end, tileT& dest, posT dbegin) {
            assert(&source != &dest);
            assert(0 <= begin.x && 0 <= begin.y);
            assert(begin.x <= end.x && begin.y <= end.y);
            assert(end.x <= source.width() && end.y <= source.height());
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
        // TODO: tileT_fill_arg / random_fill goes here...

    } // namespace tileT_utils

    // TODO: As to pattern modification:
    // clipboard-based copy/paste is wasteful... enable in-memory mode (tileT-based)...

    // TODO: begin,end?
    // https://conwaylife.com/wiki/Run_Length_Encoded
    inline std::string to_RLE_str(const tileT& tile) {
        // (wontfix) consecutive '$'s are not combined.
        std::string str;
        int last_nl = 0;
        for (int y = 0; y < tile.height(); ++y) {
            if (y != 0) {
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
            for (bool b : std::span(tile.line(y), tile.width())) {
                if (v != b) {
                    flush();
                    v = b;
                }
                ++c;
            }
            if (v != 0) {
                flush();
            }
        }
        return str;
    }

    inline std::string to_RLE_str(const tileT& t, const ruleT& r) {
        // std::format("x = {}, y = {}, rule = {}\n{}!", t.width(), t.height(), to_MAP_str(r), to_RLE_str(t));
        return "x = " + std::to_string(t.width()) + ", y = " + std::to_string(t.height()) +
               ", rule = " + to_MAP_str(r) + '\n' + to_RLE_str(t) + '!';
    }

    // TODO: re-implement...
    // TODO: return optional<rule>?
    inline tileT from_RLE_str(std::string_view text) {
        const char *str = text.data(), *end = str + text.size();

        // TODO: temp; skip first line; test only...
        if (*str == 'x') {
            while (str != end && *str != '\n') {
                ++str;
            }
            ++str;
        }

        auto take = [&str, end]() -> std::pair<int, char> {
            while (str != end && (*str == '\n' || *str == '\r' || *str == ' ')) {
                ++str;
            }
            if (str == end) {
                return {1, '!'};
            }

            int n = 1;
            if (*str >= '1' && *str <= '9') {
                auto [ptr, ec] = std::from_chars(str, end, n);
                if (ec == std::errc{}) {
                    str = ptr;
                }
            }
            switch (*str++) {
            case 'b': return {n, 'b'};
            case 'o': return {n, 'o'};
            case '$': return {n, '$'};
            default: return {n, '!'};
            }
        };

        std::vector<std::vector<bool>> lines;
        lines.emplace_back();
        for (;;) {
            auto [n, c] = take();
            if (c == '!') {
                break;
            }
            switch (c) {
            case 'b':
            case 'o':
                for (int i = 0; i < n; ++i) {
                    lines.back().push_back(c == 'o');
                }
                break;
            case '$':
                for (int i = 0; i < n; ++i) {
                    lines.emplace_back();
                }
                break;
            }
        }

        int max_width = 1;
        for (auto& line : lines) {
            max_width = std::max(max_width, (int)line.size());
        }
        assert(!lines.empty());
        tileT tile({.width = max_width, .height = (int)lines.size()});
        for (int i = 0; i < lines.size(); ++i) {
            for (int j = 0; j < lines[i].size(); ++j) {
                tile.line(i)[j] = lines[i][j];
            }
        }
        return tile;
    }

#ifndef NDEBUG
    // TODO: refine...
    namespace _misc {
        inline const bool test_RLE_str = [] {
            tileT tile({.width = 32, .height = 32});
            // TODO: use random_fill...
            for (auto& b : tile.data()) {
                b = rand() & 1;
            }
            const std::string str = to_RLE_str(tile);
            const tileT tile2 = from_RLE_str(str);
            assert(tile == tile2);
            return true;
        }();
    } // namespace _misc
#endif
} // namespace legacy
