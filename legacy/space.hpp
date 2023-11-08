#pragma once

#include "rule.hpp"
#include "tile.hpp"

namespace legacy {

    // TODO: tileT_data????
    // TODO: generally not too useful unless area copy/paste is supported...
    // TODO: not prioritized above rule-editor, which is still in trouble...
    struct spaceT {

        using tileP = tileT*;
        struct space_tile {
            tileP q, w, e;
            tileP a, d;
            tileP z, x, c;

            tileT tile;
        };

        rectT tile_size;

        tileT void_tile;
        tileT shared;

        static constexpr int min_width = 20, min_height = 20;
        // TODO: max_x?
        // TODO: refine...
        static rectT actual_size(rectT size) {
            const int w = size.width, h = size.height;
            while (size.width < min_width) {
                size.width += w;
            }
            while (size.height < min_height) {
                size.height += h;
            }
            return size;
        }

        // TODO: the size should be shared...

        void set_background(tileT& back) const {}

        void load(int x, int y, tileT& tile) const {}
    };

} // namespace legacy
