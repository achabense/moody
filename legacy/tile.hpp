#pragma once

#include <span>

#include "rule.hpp"

namespace legacy {
    static_assert(INT_MAX >= INT32_MAX);

    struct rectT {
        int width, height;
        int area() const { return width * height; }
        friend bool operator==(const rectT&, const rectT&) = default;
    };

    // TODO: add basic noexcept annotation?
    // TODO: when is it needed to [return] a tile?
    // TODO: explain layout... reorganize for better readability...
    class tileT {
        rectT m_size; // observable width and height.
        bool* m_data; // layout: [height+2][width]|[height+2][2].

    public:
        explicit tileT(rectT size) : m_size(size) {
            assert(m_size.width > 1 && m_size.height > 1);
            m_data = new bool[(m_size.width + 2) * (m_size.height + 2)]{};
        }

        // conceptually write-only after this call...
        void resize(rectT size) {
            if (m_size != size) {
                tileT resized(size);
                swap(resized);
            }
        }

        ~tileT() { delete[] m_data; }

        // TODO: should empty state be supported?
        // TODO: support copy when needed...
        tileT() = delete;
        tileT(const tileT&) = delete;
        tileT& operator=(const tileT&) = delete;

        void swap(tileT& other) {
            std::swap(m_size, other.m_size);
            std::swap(m_data, other.m_data);
        }

    public:
        rectT size() const { return m_size; }

        int width() const { return m_size.width; }
        int height() const { return m_size.height; }
        int area() const { return m_size.area(); }

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

        // TODO: ? whether to expose consecutive data?
        // TODO: is data a proper name?
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
        const tileT& gather( // clang-format off
            const tileT& q, const tileT& w, const tileT& e,
            const tileT& a, /*   *this   */ const tileT& d,
            const tileT& z, const tileT& x, const tileT& c
        ) { // clang-format on
            // assert m_shape == *.m_shape.
            const int width = m_size.width, height = m_size.height;

            // TODO: copy_line method? (std::copy_n is less clear than memcpy here)
            // (why does memxxx take dest as first arg, while copy_n take dest as last?)
            // memcpy(_line(0), w._line(height), width * sizeof(bool));
            // memcpy(_line(height + 1), x._line(1), width * sizeof(bool));
            std::copy_n(w._line(height), width, _line(0));
            std::copy_n(x._line(1), width, _line(height + 1));

            _set_lr(0, q._line(height)[width - 1], e._line(height)[0]);
            _set_lr(height + 1, z._line(1)[width - 1], c._line(1)[0]);
            for (int _y = 1; _y <= height; ++_y) {
                _set_lr(_y, a._line(_y)[width - 1], d._line(_y)[0]);
            }

            return *this;
        }

        // TODO: This could be used to support constraint gathering...
        // TODO: go back to template when needed...
        // Relying on width > 1 (which is a reasonable requirement)
        // I hate this function, it is the payment for consecutive data...
        void apply(const ruleT& rule, tileT& dest) const {
            // pre: already gathered ???<TODO>, which is untestable.
            assert(this != &dest);
            dest.resize(m_size);

            const int width = m_size.width, height = m_size.height;

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

    inline namespace tileT_utils {
        inline int count(const tileT& tile) {
            int c = 0;
            for (bool b : tile.data()) {
                c += b;
            }
            return c;
        }

        inline int count_diff(const tileT& l, const tileT& r) {
            assert(l.size() == r.size());
            int c = 0;
            const bool* l_data = l.line(0); // TODO: was begin()
            for (bool b : r.data()) {
                c += (b != *l_data++);
            }
            return c;
        }

        // TODO: is this copy or paste???
        enum class copyE { Value, Or, Xor };
        template <copyE mode = copyE::Value>
        inline void copy(const tileT& source, int sx, int sy, int width, int height, tileT& dest, int dx, int dy) {
            assert(&source != &dest);
            // TODO: precondition...

            for (int y = 0; y < height; ++y) {
                const bool* s = source.line(sy + y) + sx;
                bool* d = dest.line(dy + y) + dx;
                for (int x = 0; x < width; ++x) {
                    if constexpr (mode == copyE::Value) {
                        d[x] = s[x];
                    } else if constexpr (mode == copyE::Or) {
                        d[x] |= s[x];
                    } else {
                        static_assert(mode == copyE::Xor);
                        d[x] ^= s[x];
                    }
                }
            }
        }

        inline void clone(const tileT& source, tileT& dest) {
            assert(&source != &dest);

            dest.resize(source.size()); // <????
            // ~ Why is there no ranges::copy(range,range) version?
            std::ranges::copy(source.data(), dest.data().begin());
        }

        // TODO: is the name meaningful?
        // Return smallest rectT ~ (>= target) && (% period == 0)
        inline rectT upscale(rectT target, rectT period) {
            const auto upscale = [](int target, int period) { //
                return ((target + period - 1) / period) * period;
            };

            return {.width = upscale(target.width, period.width), //
                    .height = upscale(target.height, period.height)};
        }

        inline void piece_up(const tileT& period, tileT& target) {
            assert(target.width() >= period.width());
            assert(target.height() >= period.height());
            assert(target.width() % period.width() == 0);
            assert(target.height() % period.height() == 0);

            for (int y = 0; y < target.height(); y += period.height()) {
                for (int x = 0; x < target.width(); x += period.width()) {
                    copy(period, 0, 0, period.width(), period.height(), target, x, y);
                }
            }
        }

        // TODO: tileT_fill_arg / random_fill goes here...

    } // namespace tileT_utils

    // TODO: experimental; refine...
    // TODO: together with to_MAP_str, shall be incorporated into a single header...
    inline std::string to_rle_str(const tileT& tile /*bool add_exclamation = false*/) {
        // TODO: problematic... (sub-optimal... / maybe has bugs...)
        // TODO: empty lines should be neglected; and consecutive '$'s should be combined...
        std::string str;
        bool v = 0;
        int c = 0;
        auto flush = [&]() {
            if (c != 0) {
                if (c != 1) {
                    str += std::to_string(c);
                }
                str += "bo"[v];
            }
            c = 0;
        };

        for (int y = 0; y < tile.height(); ++y) {
            for (bool b : std::span(tile.line(y), tile.width())) {
                if (v != b) {
                    flush();
                    v = b;
                }
                ++c;
            }
            flush();
            str += '$';
        }
        // TODO: when is '!' needed?
        return str;
    }
} // namespace legacy
