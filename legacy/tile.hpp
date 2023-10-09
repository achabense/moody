#pragma once

// TODO: only for accumulate...
#include <numeric>

#include "rule.hpp"

namespace legacy {
    // TODO: unify width/height or height/width...

    // [xxx] height == 2
    // [xxx] width == 3
    struct shapeT {
        int height, width;
        bool operator==(const shapeT&) const = default;
    };

    // TODO: document actual width ...
    // TODO: allow empty state...

    class tileT {
        shapeT m_shape; // observable height and width.
        bool* m_data;

        void reconstruct(tileT tile) {
            swap(tile);
        }

    public:
        static constexpr bool no_clear = false;

        tileT() = delete;

        explicit tileT(shapeT shape, bool clear = true) : m_shape(shape) {
            assert(m_shape.height > 0 && m_shape.width > 0);
            if (clear) {
                m_data = new bool[data_length()]{};
            } else {
                m_data = new bool[data_length()];
            }
        }

        void resize(shapeT shape, bool clear = true) {
            if (m_shape != shape) {
                reconstruct(tileT(shape, clear));
            }
        }

        ~tileT() {
            delete[] m_data;
        }

        tileT(const tileT& other) : tileT(other.m_shape, no_clear) {
            std::copy_n(other.m_data, data_length(), m_data);
        }

        tileT& operator=(const tileT& other) {
            if (this != &other) {
                resize(other.m_shape, no_clear);
                std::copy_n(other.m_data, data_length(), m_data);
            }
            return *this;
        }

        void swap(tileT& other) {
            std::swap(m_shape, other.m_shape);
            std::swap(m_data, other.m_data);
        }

    public:
        int height() const {
            return m_shape.height;
        }

        int width() const {
            return m_shape.width;
        }

        int area() const {
            return m_shape.height * m_shape.width;
        }

        shapeT shape() const {
            return m_shape;
        }

    private:
        // TODO: terrible...
        // [0, actual_width)
        //   [1,width]
        //   [0,width)
        int actual_height() const {
            return height() + 2;
        }
        int actual_width() const {
            return width() + 2;
        }
        int data_length() const {
            return actual_height() * actual_width();
        }

        // TODO: explain why const
        bool* _line(int _y) const {
            return m_data + _y * actual_width();
        }

    public:
        bool* line(int y) {
            assert(y >= 0 && y < m_shape.height);
            return _line(y + 1) + 1;
        }

        const bool* line(int y) const {
            assert(y >= 0 && y < m_shape.height);
            return _line(y + 1) + 1;
        }

        // TODO: clear etc...
        bool& at(int y, int x) {
            assert(y >= 0 && y < m_shape.height && x >= 0 && x < m_shape.width);
            return line(y)[x];
        }

        // TODO: bool or const bool&?
        bool at(int y, int x) const {
            assert(y >= 0 && y < m_shape.height && x >= 0 && x < m_shape.width);
            return _line(y + 1)[x + 1];
        }

    private:
        // This could be used to support arbitary-sized space. TODO: rephrase...
        const tileT& gather( // clang-format off
            const tileT* q, const tileT* w, const tileT* e,
            const tileT* a, /*    this   */ const tileT* d,
            const tileT* z, const tileT* x, const tileT* c
        ) const { // clang-format on
            // assert m_shape == *.m_shape.
            auto [height, width] = m_shape;

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
            dest.resize(m_shape);

            auto [height, width] = m_shape;
            auto _apply = [&rule, width](bool* _dest, const bool* _up, const bool* _sc, const bool* _dw) {
                for (int _x = 1; _x <= width; ++_x) {
                    // clang-format off
                    _dest[_x] = rule.map(_up[_x - 1], _up[_x], _up[_x + 1],
                                         _sc[_x - 1], _sc[_x], _sc[_x + 1],
                                         _dw[_x - 1], _dw[_x], _dw[_x + 1]);
                    // clang-format on
                }
            };

            for (int _y = 1; _y <= height; ++_y) {
                _apply(dest._line(_y), _line(_y - 1), _line(_y), _line(_y + 1));
            }
        }

        /* [[deprecated]] */ tileT apply(const ruleT& rule) const {
            // pre: already gathered.
            tileT working(m_shape, no_clear);
            apply(rule, working);
            return working;
        }

        // TODO: why member function?
        // todo: less useful than for_each...
        // todo: use bernoulli_distribution instead...
        void random_fill(double density, auto&& rnd) {
            density = std::clamp(density, 0.0, 1.0);
            auto [height, width] = m_shape;
            int area = height * width;
            bool* to_shuffle = new bool[area];

            std::fill_n(to_shuffle, area, false);
            std::fill_n(to_shuffle, area * density, true);
            std::shuffle(to_shuffle, to_shuffle + area, rnd);

            for (int y = 0; y < height; ++y) {
                memcpy(line(y), to_shuffle + y * width, width);
            }
            delete[] to_shuffle;
        }

        // TODO: heavy?
        // TODO: too trivial and app-specific?
        // for-each-line?
        int count() const {
            int cnt = 0;
            auto [height, width] = m_shape;
            for (int y = 0; y < height; ++y) {
                cnt += std::accumulate(line(y), line(y) + width, 0);
            }
            return cnt;
        }
        bool operator==(const tileT& other) const {
            if (m_shape != other.m_shape) {
                return false;
            }

            auto [height, width] = other.m_shape;
            for (int y = 0; y < height; ++y) {
                if (!memcmp(line(y), other.line(y), width)) {
                    return false;
                }
            }
            return true;
        }

        // experimental...
    };
} // namespace legacy
