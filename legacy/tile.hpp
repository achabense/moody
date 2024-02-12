#pragma once

#include <charconv>
#include <format> // TODO: utils...

#include "rule.hpp"

namespace legacy {
    static_assert(INT_MAX >= INT32_MAX);

    // TODO: explain layout.
    // TODO: add assertions about emptiness...
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
        sizeT m_size; // observable width and height.
        bool* m_data; // layout: [height+2][width+2].

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

        // TODO: rephrase...
        // conceptually write-only after this call...
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

        rangeT range() const { return {{0, 0}, {.x = m_size.width, .y = m_size.height}}; }
        bool has_range(const rangeT& range) const {
            const auto& [begin, end] = range;
            return 0 <= begin.x && begin.x <= end.x && end.x <= m_size.width && //
                   0 <= begin.y && begin.y <= end.y && end.y <= m_size.height;
        }

    private:
        bool* _line(int _y) {
            assert(_y >= 0 && _y < m_size.height + 2);
            return m_data + _y * (m_size.width + 2);
        }
        const bool* _line(int _y) const {
            assert(_y >= 0 && _y < m_size.height + 2);
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

        // TODO: avoid code duplication...
        void for_each_line(const rangeT& range, auto fn) {
            assert(has_range(range));
            const auto& [begin, end] = range;
            for (int y = begin.y; y < end.y; ++y) {
                bool* ln = line(y);
                if constexpr (requires { fn(std::span{ln + begin.x, ln + end.x}); }) {
                    fn(std::span{ln + begin.x, ln + end.x});
                } else {
                    fn(y - begin.y, std::span{ln + begin.x, ln + end.x});
                }
            }
        }

        void for_each_line(const rangeT& range, auto fn) const {
            assert(has_range(range));
            const auto& [begin, end] = range;
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

        // TODO: use is_invocable_r instead?
        void apply(const std::invocable<codeT> auto& rule, tileT& dest) const {
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
        inline const testT test_tileT = [] {
            const tileT::sizeT size{1, 1};

            tileT t_q(size), t_w(size), t_e(size);
            tileT t_a(size), t_s(size), t_d(size);
            tileT t_z(size), t_x(size), t_c(size);
            tileT dest(size);

            const ruleT rule = make_rule([](codeT) { return testT::rand() & 1; });
            for_each_code(code) {
                const auto [q, w, e, a, s, d, z, x, c] = decode(code);
                t_q.line(0)[0] = q, t_w.line(0)[0] = w, t_e.line(0)[0] = e;
                t_a.line(0)[0] = a, t_s.line(0)[0] = s, t_d.line(0)[0] = d;
                t_z.line(0)[0] = z, t_x.line(0)[0] = x, t_c.line(0)[0] = c;

                t_s.gather(t_q, t_w, t_e, t_a, t_d, t_z, t_x, t_c);
                t_s.apply(rule, dest);
                assert(dest.line(0)[0] == rule(code));
            }
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

    using rangeT_opt = std::optional<tileT::rangeT>;

    inline namespace tileT_utils {
        inline int count(const tileT& tile, const rangeT_opt& range = {}) {
            int c = 0;
            tile.for_each_line(range.value_or(tile.range()), [&c](std::span<const bool> line) {
                for (bool v : line) {
                    c += v;
                }
            });
            return c;
        }

        // TODO: is this "copy" or "paste"???
        enum class copyE { Value, Or, Xor };
        template <copyE mode = copyE::Value>
        inline void copy(tileT& dest, tileT::posT dbegin, const tileT& source, const rangeT_opt& srange_ = {}) {
            assert(&source != &dest);

            const tileT::rangeT srange = srange_.value_or(source.range());
            assert(dest.has_range({dbegin, dbegin + srange.size()}));

            source.for_each_line(srange, [&dest, &dbegin](int y, std::span<const bool> line) {
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

        // TODO: Trying to guarantee that the result is independent of std implementation...
        // Is this worthwhile? Fall back to bernoulli_distribution instead?
        inline void random_fill(tileT& tile, std::mt19937& rand, double density, const rangeT_opt& range = {}) {
            const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
            tile.for_each_line(range.value_or(tile.range()), [&](std::span<bool> line) { //
                std::ranges::generate(line, [&] { return rand() < c; });
            });
        }

        inline void clear_inside(tileT& tile, const tileT::rangeT& range /* Required */, bool v = 0) {
            tile.for_each_line(range, [v](std::span<bool> line) { std::ranges::fill(line, v); });
        }

        // TODO: mixture of tile.range() and range arg looks confusing... rename.
        inline void clear_outside(tileT& tile, const tileT::rangeT& range /* Required */, bool v = 0) {
            assert(tile.has_range(range));
            tile.for_each_line(tile.range(), [&](int y, std::span<bool> line) {
                if (y < range.begin.y || y >= range.end.y) {
                    std::ranges::fill(line, v);
                } else {
                    std::fill(line.begin(), line.begin() + range.begin.x, v);
                    std::fill(line.begin() + range.end.x, line.end(), v);
                }
            });
        }

#if 0
        tileT::rangeT shrink(const tileT& tile, const tileT::rangeT& range, bool v);

        inline int count_diff(const tileT& l, const tileT& r);

        // TODO: is the name meaningful?
        // Return smallest sizeT ~ (>= target) && (% period == 0)
        inline tileT::sizeT upscale(tileT::sizeT target, tileT::sizeT period) {
            const auto upscale = [](int target, int period) { //
                return ((target + period - 1) / period) * period;
            };

            return {.width = upscale(target.width, period.width), //
                    .height = upscale(target.height, period.height)};
        }

        inline void piece_up(const tileT& period, tileT& target);
#endif
    } // namespace tileT_utils

    // https://conwaylife.com/wiki/Run_Length_Encoded
    inline std::string to_RLE_str(const tileT& tile, const rangeT_opt& range = {}) {
        // (wontfix) consecutive '$'s are not combined.
        std::string str;
        int last_nl = 0;
        tile.for_each_line(range.value_or(tile.range()), [&str, &last_nl](int y, std::span<const bool> line) {
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

    inline std::string to_RLE_str(const ruleT& rule, const tileT& tile, const rangeT_opt& range_ = {}) {
        const tileT::rangeT range = range_.value_or(tile.range());
        return std::format("x = {}, y = {}, rule = {}\n{}!", range.width(), range.height(), to_MAP_str(rule),
                           to_RLE_str(tile, range));
    }

    // TODO: whether to skip lines leading with '#'?
    inline tileT from_RLE_str(std::string_view text, tileT::sizeT max_size) {
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

#ifdef ENABLE_TESTS
    // TODO: update when from_RLE_str accepts x = ... line...
    namespace _tests {
        inline const testT test_RLE_str = [] {
            tileT tile({.width = 32, .height = 60});
            // TODO: better source of rand...
            std::mt19937 rand{};
            random_fill(tile, rand, 0.5);
            assert(tile == from_RLE_str(to_RLE_str(tile), tile.size()));
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

} // namespace legacy
