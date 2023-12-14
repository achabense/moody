#pragma once

// TODO: only for accumulate...
#include <numeric>
// #include <span>

#include "rule.hpp"

namespace legacy {
    static_assert(INT_MAX >= INT32_MAX);

    struct rectT {
        int width, height;
        friend bool operator==(const rectT&, const rectT&) = default;
    };

    // TODO: add area() for rectT?
    // TODO: add basic noexcept annotation?
    // TODO: when is it needed to [return] a tile?
    // TODO: explain layout... reorganize for better readibility...
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

        // TODO: ? whether to expose consecutive data?
        // std::span<bool> data() { return {line(0), line(0) + area()}; }
        // std::span<const bool> data() const { return {line(0), line(0) + area()}; }

        bool* begin() { return line(0); }
        const bool* begin() const { return line(0); }

        bool* end() { return begin() + area(); }
        const bool* end() const { return begin() + area(); }

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

        // TODO: what's the preferred form of public interface?
    public:
        // TODO: This could be used to support constraint gathering...
        // Relying on width > 1 (which is a reasonable requirement)
        // I hate this function, it is the payment for consecutive data...
        void _apply(const auto& rulefn /*bool(codeT)*/, tileT& dest) const {
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
                _dest[0] = rulefn(encode(_q, _up[0], _up[1],
                                         _a, _ct[0], _ct[1],
                                         _z, _dw[0], _dw[1]));
                for (int x = 1; x < width - 1; ++x) {
                    _dest[x] = rulefn(encode(_up[x - 1], _up[x], _up[x + 1],
                                             _ct[x - 1], _ct[x], _ct[x + 1],
                                             _dw[x - 1], _dw[x], _dw[x + 1]));
                }
                _dest[width - 1] = rulefn(encode(_up[width - 2], _up[width - 1], _e,
                                                 _ct[width - 2], _ct[width - 1], _d,
                                                 _dw[width - 2], _dw[width - 1], _c));
                // clang-format on
            }
        }

    public:
        void apply(const ruleT& rule, tileT& dest) const { //
            _apply(rule, dest);
        }

        int count() const { //
            return std::accumulate(begin(), end(), 0);
        }
    };

    inline void copy(const tileT& source, int sx, int sy, int width, int height, tileT& dest, int dx, int dy) {
        assert(&source != &dest);
        // TODO: precondition...

        for (int y = 0; y < height; ++y) {
            std::copy_n(source.line(sy + y) + sx, width, dest.line(dy + y) + dx);
        }
    }

    // TODO: experimental; refine...
    // TODO: together with to_MAP_str, shall be incorporated into a single header...
    inline std::string to_rle_str(const tileT& tile) {
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

        const bool* data = tile.begin();
        for (int y = 0; y < tile.height(); ++y) {
            for (int x = 0; x < tile.width(); ++x) {
                bool v2 = *data++;
                if (v != v2) {
                    flush();
                    v = v2;
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
