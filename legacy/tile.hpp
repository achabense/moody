#pragma once

#include <charconv>
#include <format> // TODO: utils...
#include <random> // TODO: utils...
#include <span>

#include "rule.hpp"

namespace legacy {
    static_assert(INT_MAX >= INT32_MAX);

    // TODO: explain layout... reorganize for better readability...
    // TODO: add assertions, especially about empty tileT... (e.g. currently even operator== is invalid)
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

    private:
        sizeT m_size; // observable width and height.
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

        explicit tileT(const sizeT& size) : tileT() {
            if (size.width > 0 && size.height > 0) {
                m_size = size;
                m_data = new bool[(m_size.width + 2) * (m_size.height + 2)]{};
            }
        }
        ~tileT() { delete[] m_data; }

        // TODO: rephrase...
        // conceptually write-only after this call...
        void resize(const sizeT& size) {
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

        static posT begin_pos() { return {0, 0}; }
        posT end_pos() const { return {.x = m_size.width, .y = m_size.height}; }
        bool verify_pos(posT begin, posT end) const {
            return 0 <= begin.x && begin.x <= end.x && end.x <= m_size.width && //
                   0 <= begin.y && begin.y <= end.y && end.y <= m_size.height;
        }

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

        // TODO: avoid code duplication...
        void for_each_line(posT begin, posT end, auto fn) {
            assert(verify_pos(begin, end));

            for (int y = begin.y; y < end.y; ++y) {
                bool* ln = line(y);
                if constexpr (requires { fn(std::span{ln + begin.x, ln + end.x}); }) {
                    fn(std::span{ln + begin.x, ln + end.x});
                } else {
                    fn(y - begin.y, std::span{ln + begin.x, ln + end.x});
                }
            }
        }

        void for_each_line(posT begin, posT end, auto fn) const {
            assert(verify_pos(begin, end));

            for (int y = begin.y; y < end.y; ++y) {
                const bool* ln = line(y);
                if constexpr (requires { fn(std::span{ln + begin.x, ln + end.x}); }) {
                    fn(std::span{ln + begin.x, ln + end.x});
                } else {
                    fn(y - begin.y, std::span{ln + begin.x, ln + end.x});
                }
            }
        }

        // TODO: at(posT)?
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
        // TODO: boundless space (as well as & tiling) are no longer planned for the first release
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

        // TODO: use is_invocable_r instead?
        // I hate this function, it is the payment for consecutive data...
        void apply(const std::invocable<codeT> auto& rule, tileT& dest) const {
            // There is supposed to be a call to `gather` before calling `apply`.
            // (Which is untestable and must be guaranteed by the callers.)

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

                    _dest[0] = rule(encode({_q, _w, _e, _a, _s, _d, _z, _x, _c}));
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
                _dest[0] = rule(encode({_q, _up[0], _up[1],
                                        _a, _ct[0], _ct[1],
                                        _z, _dw[0], _dw[1]}));
                for (int x = 1; x < width - 1; ++x) {
                    _dest[x] = rule(encode({_up[x - 1], _up[x], _up[x + 1],
                                            _ct[x - 1], _ct[x], _ct[x + 1],
                                            _dw[x - 1], _dw[x], _dw[x + 1]}));
                }
                _dest[width - 1] = rule(encode({_up[width - 2], _up[width - 1], _e,
                                                _ct[width - 2], _ct[width - 1], _d,
                                                _dw[width - 2], _dw[width - 1], _c}));
                // clang-format on
            }
        }

        // TODO: refine...
        friend bool operator==(const tileT& l, const tileT& r) { //
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

#ifndef NDEBUG
    namespace _misc::tests {
        // TODO: enhance this test?
        inline const bool test_tileT = [] {
            const tileT::sizeT size{1, 1};
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
    } // namespace _misc::tests
#endif

    inline namespace tileT_utils {
        inline int count(const tileT& tile) {
            int c = 0;
            tile.for_each_line(tile.begin_pos(), tile.end_pos(), [&c](std::span<const bool> line) {
                for (bool v : line) {
                    c += v;
                }
            });
            return c;
        }

        // TODO: is this "copy" or "paste"???
        enum class copyE { Value, Or, Xor };
        template <copyE mode = copyE::Value>
        inline void copy(const tileT& source, tileT::posT begin, tileT::posT end, tileT& dest, tileT::posT dbegin) {
            assert(&source != &dest);
            assert(dest.verify_pos(dbegin, {dbegin.x + (end.x - begin.x), dbegin.y + (end.y - begin.y)}));

            source.for_each_line(begin, end, [&dest, &dbegin](int y, std::span<const bool> line) {
                bool* d = dest.line(dbegin.y + y) + dbegin.x;
                for (bool v : line) {
                    if constexpr (mode == copyE::Value) {
                        *d++ = v;
                    } else if constexpr (mode == copyE::Or) {
                        *d++ |= v;
                    } else {
                        static_assert(mode == copyE::Xor);
                        *d++ ^= v;
                    }
                }
            });
        }

        template <copyE mode = copyE::Value>
        inline void copy(const tileT& source, tileT& dest, tileT::posT dbegin) {
            copy<mode>(source, source.begin_pos(), source.end_pos(), dest, dbegin);
        }

        // TODO: add param const...
        // TODO: is the algo correct?
        // TODO: explain why not using bernoulli_distribution
        inline void random_fill(tileT& tile, std::mt19937& rand, double density, tileT::posT begin, tileT::posT end) {
            const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
            tile.for_each_line(begin, end, [&](std::span<bool> line) { //
                std::ranges::generate(line, [&] { return rand() < c; });
            });
        }

        inline void random_fill(tileT& tile, std::mt19937& rand, double density) {
            random_fill(tile, rand, density, tile.begin_pos(), tile.end_pos());
        }

        inline void clear_inside(tileT& tile, tileT::posT begin, tileT::posT end, bool v) {
            tile.for_each_line(begin, end, [v](std::span<bool> line) { std::ranges::fill(line, v); });
        }

        inline void clear_outside(tileT& tile, tileT::posT begin, tileT::posT end, bool v) {
            assert(tile.verify_pos(begin, end));
            tile.for_each_line(tile.begin_pos(), tile.end_pos(), [&](int y, std::span<bool> line) {
                if (y < begin.y || y >= end.y) {
                    std::ranges::fill(line, v);
                } else {
                    std::fill(line.begin(), line.begin() + begin.x, v);
                    std::fill(line.begin() + end.x, line.end(), v);
                }
            });
        }

#if 0
        void shrink(const tileT& tile, tileT::posT& begin, tileT::posT& end, bool v);

        inline int count_diff(const tileT& l, const tileT& r);

        // TODO: is the name meaningful?
        // Return smallest sizeT ~ (>= target) && (% period == 0)
        inline tileT::sizeT upscale(const tileT::sizeT& target, const tileT::sizeT& period) {
            const auto upscale = [](int target, int period) { //
                return ((target + period - 1) / period) * period;
            };

            return {.width = upscale(target.width, period.width), //
                    .height = upscale(target.height, period.height)};
        }

        inline void piece_up(const tileT& period, tileT& target);

        inline void piece_up(const tileT& period, tileT& target, tileT::posT begin /*[*/, tileT::posT end /*)*/);
#endif
    } // namespace tileT_utils

    // TODO: As to pattern modification:
    // clipboard-based copy/paste is wasteful... enable in-memory mode (tileT-based)...

    // https://conwaylife.com/wiki/Run_Length_Encoded
    inline std::string to_RLE_str(const tileT& tile, tileT::posT begin, tileT::posT end) {
        // (wontfix) consecutive '$'s are not combined.
        std::string str;
        int last_nl = 0;
        tile.for_each_line(begin, end, [&str, &last_nl](int y, std::span<const bool> line) {
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
            for (const bool b : line) {
                if (v != b) {
                    flush();
                    v = b;
                }
                ++c;
            }
            flush(); // (wontfix) trailing 0s are not omitted.
        });
        return str;
    }

    inline std::string to_RLE_str(const tileT& tile) { //
        return to_RLE_str(tile, tile.begin_pos(), tile.end_pos());
    }

    inline std::string to_RLE_str(const ruleT& rule, const tileT& tile) {
        return std::format("x = {}, y = {}, rule = {}\n{}!", tile.width(), tile.height(), to_MAP_str(rule),
                           to_RLE_str(tile));
    }

    inline std::string to_RLE_str(const ruleT& rule, const tileT& tile, tileT::posT begin, tileT::posT end) {
        return std::format("x = {}, y = {}, rule = {}\n{}!", end.x - begin.x, end.y - begin.y, to_MAP_str(rule),
                           to_RLE_str(tile, begin, end));
    }

    // TODO: whether to skip lines leading with '#'?
    inline tileT from_RLE_str(std::string_view text, const tileT::sizeT& max_size) {
        // TODO: do the real parse... (especially "rule = ..." part)
        if (text.starts_with('x')) {
            text.remove_prefix(std::min(text.size(), text.find('\n')));
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

        // TODO: exceptions like this should be reported via popup?
        if (width == 1 && height == 1) {
            throw std::runtime_error("TODO... what msg? whether to throw at all?");
        }
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
    namespace _misc::tests {
        inline const bool test_RLE_str = [] {
            tileT tile({.width = 32, .height = 60});
            // TODO: better source of rand...
            std::mt19937 rand{};
            random_fill(tile, rand, 0.5);
            assert(tile == from_RLE_str(to_RLE_str(tile), tile.size()));
            return true;
        }();
    } // namespace _misc::tests
#endif
} // namespace legacy
