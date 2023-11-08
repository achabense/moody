#pragma once

#include <unordered_map>

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
        struct posT {
            uint32_t x, y;
            friend bool operator==(const posT&, const posT&) = default;
            // Using the same type as hasher for convenience:
            size_t operator()(const posT& pos) const {
                return std::hash<uint64_t>{}((uint64_t(pos.x) << 32) | pos.y);
            }
        };

        rectT tile_size;

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

        std::unordered_map<posT, space_tile, posT> m_space;
        tileT void_tile;
        tileT shared;
        int m_gen;

        // TODO: the size should be shared...

        void set_background(tileT& back) const {}

        void load(int x, int y, tileT& tile) const {}

        void run(const ruleT& rule) {
            // Step 0: ...
            // TODO: utilize speed of light for less frequent updates...
            // ...
            // Step 1: ...
            void_tile.gather();
            for (auto& [p, tile] : m_space) {
                tile.tile.gather(/*...*/);
            }
            // Step 2: ...
            void_tile.apply(rule, shared);
            void_tile.swap(shared);
            for (auto& [p, tile] : m_space) {
                tile.tile.apply(rule, shared);
                tile.tile.swap(shared);
            }
            // ...
            ++m_gen;
        }
    };

} // namespace legacy
