#pragma once
#include "rule.hpp"

namespace aniso {
    struct vecT {
        int x, y;

        int xy() const { return x * y; }

        friend bool operator==(const vecT&, const vecT&) = default;
        friend vecT operator+(const vecT& a, const vecT& b) { return {.x = a.x + b.x, .y = a.y + b.y}; }
        friend vecT operator-(const vecT& a, const vecT& b) { return {.x = a.x - b.x, .y = a.y - b.y}; }
        friend vecT operator/(const vecT& a, int b) { return {.x = a.x / b, .y = a.y / b}; }
        friend vecT operator*(const vecT& a, int b) { return {.x = a.x * b, .y = a.y * b}; }
        friend vecT operator/(const vecT& a, double b) = delete;
        friend vecT operator*(const vecT& a, double b) = delete;

        [[nodiscard]] vecT plus(int dx, int dy) const { return {.x = x + dx, .y = y + dy}; }
        bool both_gteq(const vecT& b) const { return x >= b.x && y >= b.y; } // >=
        bool both_lteq(const vecT& b) const { return x <= b.x && y <= b.y; } // <=
        bool both_lt(const vecT& b) const { return x < b.x && y < b.y; }     // <
    };

    inline vecT clamp(const vecT& p, const vecT& min, const vecT& max) { // []
        assert(min.both_lteq(max));
        return {.x = std::clamp(p.x, min.x, max.x), .y = std::clamp(p.y, min.y, max.y)};
    }

    struct rangeT {
        vecT begin, end; // [)

        vecT size() const {
            assert(begin.both_lteq(end));
            return end - begin;
        }

        bool empty() const {
            assert(begin.both_lteq(end));
            return begin.x == end.x || begin.y == end.y;
        }
    };

    inline rangeT common(const rangeT& a, const rangeT& b) {
        const vecT begin = clamp(a.begin, b.begin, b.end);
        const vecT end = clamp(a.end, b.begin, b.end);
        if (begin.both_lt(end)) {
            return {begin, end};
        } else {
            return {};
        }
    }

    namespace _misc {
        // Should be non-empty.
        template <class T>
        struct tile_ref_ {
            vecT size;
            int stride; // Number of elements (instead of bytes).
            T* data;    // Non-owning; points at [0][0].

            operator tile_ref_<const T>() const
                requires(!std::is_const_v<T>)
            {
                return tile_ref_<const T>{size, stride, data};
            }

            T* line(int y) const {
                assert(y >= 0 && y < size.y);
                return data + stride * y;
            }
            T& at(vecT pos) const {
                assert(pos.both_gteq({0, 0}) && pos.both_lt(size));
                return *(data + stride * pos.y + pos.x);
            }

            bool has_range(const rangeT& range) const {
                return range.begin.both_gteq({0, 0}) && range.end.both_lteq(size);
            }
            [[nodiscard]] tile_ref_ clip(const rangeT& range) const {
                assert(!range.empty() && has_range(range));
                return {range.size(), stride, data + range.begin.x + stride * range.begin.y};
            }

            void for_each_line(const auto& fn) const {
                T* data = this->data;
                for (int y = 0; y < size.y; ++y, data += stride) {
                    if constexpr (requires { fn((int)y, std::span{data, data + size.x}); }) {
                        fn((int)y, std::span{data, data + size.x});
                    } else {
                        static_assert(requires { fn(std::span{data, data + size.x}); });
                        fn(std::span{data, data + size.x});
                    }
                }
            }

            void for_all_data(const auto& fn) const {
                static_assert(requires { fn(std::span{data, data + size.x * size.y}); });
                if (size.x == stride) {
                    fn(std::span{data, data + size.x * size.y});
                } else {
                    this->for_each_line(fn);
                }
            }

            template <class U>
            void for_all_data_vs(const tile_ref_<U>& b, const auto& fn) const {
                static_assert(requires { fn(data, b.data, 1); });
                assert(size == b.size);

                if (size.x == stride && b.size.x == b.stride) {
                    fn(data, b.data, size.x * size.y);
                    return;
                }

                T* this_data = data;
                U* that_data = b.data;
                for (int y = 0; y < size.y; ++y, this_data += stride, that_data += b.stride) {
                    fn(this_data, that_data, size.x);
                }
            }

            // TODO: add operator==?
        };
    } // namespace _misc

    using tile_ref = _misc::tile_ref_<bool>;
    using tile_const_ref = _misc::tile_ref_<const bool>;
} // namespace aniso
