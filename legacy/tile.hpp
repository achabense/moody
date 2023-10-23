#pragma once

// TODO: only for accumulate...
#include <numeric>

#include "rule.hpp"

namespace legacy {
    struct rectT {
        int width, height;
        bool operator==(const rectT&) const = default;
    };

    class tileT {
        rectT m_size; // observable width and height.
        bool* m_data;

    public:
        tileT() = delete;

        explicit tileT(rectT size) : m_size(size) {
            assert(m_size.width > 0 && m_size.height > 0);
            m_data = new bool[padded_area()]{/* false... */};
        }

        void resize(rectT size) {
            if (m_size != size) {
                tileT resized(size);
                swap(resized);
            }
        }

        ~tileT() {
            delete[] m_data;
        }

        tileT(const tileT&) = delete;
        tileT& operator=(const tileT&) = delete;

        void swap(tileT& other) {
            std::swap(m_size, other.m_size);
            std::swap(m_data, other.m_data);
        }

    public:
        rectT size() const {
            return m_size;
        }

        int width() const {
            return m_size.width;
        }

        int height() const {
            return m_size.height;
        }

        int area() const {
            return m_size.width * m_size.height;
        }

    private:
        // TODO: explain padding
        int padded_height() const {
            return height() + 2;
        }
        int padded_width() const {
            return width() + 2;
        }
        int padded_area() const {
            return padded_height() * padded_width();
        }

        // TODO: explain why const
        bool* _line(int _y) const {
            assert(_y >= 0 && _y < padded_height());
            return m_data + _y * padded_width();
        }

    public:
        // TODO: return span instead?
        bool* line(int y) {
            assert(y >= 0 && y < height());
            return _line(y + 1) + 1;
        }

        const bool* line(int y) const {
            assert(y >= 0 && y < height());
            return _line(y + 1) + 1;
        }

    private:
        // This could be used to support boundless space.
        const tileT& gather( // clang-format off
            const tileT* q, const tileT* w, const tileT* e,
            const tileT* a, /*    this   */ const tileT* d,
            const tileT* z, const tileT* x, const tileT* c
        ) const { // clang-format on
            // assert m_shape == *.m_shape.
            int width = m_size.width, height = m_size.height;

            for (int _x = 1; _x <= width; _x++) {
                _line(0)[_x] = w->_line(height)[_x];
                _line(height + 1)[_x] = x->_line(1)[_x];
            }
            for (int _y = 1; _y <= height; _y++) {
                _line(_y)[0] = a->_line(_y)[width];
                _line(_y)[width + 1] = d->_line(_y)[1];
            }
            _line(0)[0] = q->_line(height)[width];
            _line(0)[width + 1] = e->_line(height)[1];
            _line(height + 1)[0] = z->_line(1)[width];
            _line(height + 1)[width + 1] = c->_line(1)[1];

            return *this;
        }

    public:
        // Torus.
        const tileT& gather() const {
            gather(this, this, this, this, this, this, this, this);
            return *this;
        }

        void apply(const ruleT& rule, tileT& dest) const {
            // pre: already gathered ???<TODO>, which is untestable.
            assert(this != &dest);
            dest.resize(m_size);

            auto _apply = [&rule, width = width()](bool* _dest, const bool* _up, const bool* _sc, const bool* _dw) {
                for (int _x = 1; _x <= width; ++_x) {
                    // clang-format off
                    _dest[_x] = rule.map(_up[_x - 1], _up[_x], _up[_x + 1],
                                         _sc[_x - 1], _sc[_x], _sc[_x + 1],
                                         _dw[_x - 1], _dw[_x], _dw[_x + 1]);
                    // clang-format on
                }
            };

            for (int _y = 1; _y <= height(); ++_y) {
                _apply(dest._line(_y), _line(_y - 1), _line(_y), _line(_y + 1));
            }
        }

        int count() const {
            int count = 0;
            for (int y = 0; y < height(); ++y) {
                count += std::accumulate(line(y), line(y) + width(), 0);
            }
            return count;
        }
        bool operator==(const tileT& other) const {
            if (m_size != other.m_size) {
                return false;
            }

            for (int y = 0; y < height(); ++y) {
                if (!memcmp(line(y), other.line(y), width())) {
                    return false;
                }
            }
            return true;
        }
    };
} // namespace legacy
