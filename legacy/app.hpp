#pragma once

#include <random>

#include "rule.hpp"
#include "serialize.hpp"
#include "tile.hpp"

namespace legacy::app {
    // TODO: allow null state for tile???


    // source...
    struct tile_source {
        bool apply = true; // true: do nothing...

        shapeT shape;
        tileT data; // TODO: vector?

        std::mt19937_64 m_rand{0};
        bool using_rand = true; // true: m_rand false: m_data.
        bool lock_rand = true;
        float density{0.5};
    };

    class single_tile {
        ruleT m_rule;

        tileT m_tile;
        tileT m_side; // invisible.

        int m_gen = 0;

    public:
        single_tile(tile_source& source);

        void reset_rule(const ruleT& rule, tile_source& source) {
            m_rule = rule;
            reset_tile(source);
        }

        void reset_tile(tile_source& source) {
            if (source.apply) {
                if (source.using_rand) {
                    if (source.lock_rand) {
                        m_tile.random_fill(source.density, std::mt19937_64{source.m_rand});
                    } else {
                        m_tile.random_fill(source.density, source.m_rand);
                    }
                } else {
                    m_tile = source.data;
                }
                m_gen = 0;
            }
        }

        void run(int count) {
            for (int i = 0; i < count; ++i) {
                m_tile.gather().apply(m_rule, m_side);
                m_tile.swap(m_side);
            }
            m_gen += count;
        }

        struct stateT {
            const ruleT& rule;
            const tileT& tile;
            int gen;
        };

        stateT state() const {
            return {m_rule, m_tile, m_gen};
        }
    };

} // namespace legacy::app
