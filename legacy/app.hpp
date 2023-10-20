#pragma once

#include <random>

#include "rule.hpp"
#include "serialize.hpp"
#include "symmetry.hpp"
#include "tile.hpp"

// TODO: partition traits... e.g. can ABS make sense, or can FLIP make sense...
// TODO: partial rule (e.g. collect from tile; the rest is generated from another generator...)

////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: allow resizing the grid.
// TODO: rule editor... (based on mini-window, click the pixel to set...)..
// TODO: where to add gol?
// TODO: add pace.
// TODO: (important) small window...

// TODO: notify on quit...
// TODO!!!!!support more run-mode...
// TODO: restart should...
// TODO: run_extra..
// TODO: some settings should leave states in paused state...
// TODO: rnd-mode for tile...(stable vs arbitary...)
// TODO: right click to enable/disable miniwindow...
// TODO: fine-grained rule edition...
// TODO: file container. easy ways to add fav...

class tile_filler {
    std::mt19937_64 m_rand;

public:
    explicit tile_filler(uint64_t seed) : m_rand{seed} {}

    float density = 0.5;

    void disturb() { // TODO: explain...
        (void)m_rand();
    }
    void random_fill(legacy::tileT& tile) const {
        tile.random_fill(density, std::mt19937_64{m_rand});
    }
};

// TODO: support inc,dec, next_perm, prev_perm...
// ugh, horrible, this seems belong to editor... (as maker seems just a partition generator...)
// TODO: density is actually for generation, not matching with cursor now...
// TODO: editor, very hard...
// TODO: how to support user-defined partition?
// TODO: what's the relation with analyzer/ editor?
// TODO: how to interact with recorder automatically?
struct rule_maker {
    void random_fill(bool* begin, int size, int count) {
        assert(size > 0);
        count = std::clamp(count, 0, size);
        std::fill_n(begin, size, false);
        std::fill_n(begin, count, true);
        std::shuffle(begin, begin + size, m_rand);
    }

    bool lock_sym_spatial = true;
    bool lock_sym_state = true;

    std::mt19937_64 m_rand;
    // TODO: "density" might not be suitable name...
    // density ¡Ê [0, max_density]
    int density = 0; // TODO: rename. TODO: revert density setting?

    // TODO: aside from filters, the problem is, [what] to support?
    // TODO: partially symmetric rules?
    enum generator : int {
        none, // arbitrary 512 str, not used.
        sub_spatial,
        sub_spatial2,
        spatial,
        spatial_paired,
        spatial_state,
        perm
    } g_mode = spatial; // TODO: how to control this?

    // TODO: the enum is problematic...
    int interpret_as = legacy::ABS;

    explicit rule_maker(uint64_t seed) : m_rand{seed} {
        density = max_density() * 0.3;
    }
    void disturb() { // TODO: explain...
        (void)m_rand();
    }

    const legacy::partitionT& current_partition() const {
        switch (g_mode) {
        case none: // TODO: extremely inefficient...
            return legacy::partition::none;
        case sub_spatial:
            return legacy::partition::sub_spatial;
        case sub_spatial2:
            return legacy::partition::sub_spatial2;
        case spatial:
            return legacy::partition::spatial;
        case spatial_paired:
            return legacy::partition::spatial_paired;
        case spatial_state:
            return legacy::partition::spatial_state;
        case perm:
            return legacy::partition::permutation;
        default:
            abort(); // TODO ...
        }
    }
    int max_density() const {
        return current_partition().k();
    }

    legacy::ruleT make() {
        legacy::ruleT::array_base grule{}; // TODO: is it safe not to do value init?
        random_fill(grule.data(), max_density(), density);
        legacy::ruleT::array_base rule = current_partition().dispatch_from(grule);
        return legacy::ruleT(rule, (legacy::interpret_mode)interpret_as);
    }
};

// TODO: support shifting...
// TODO: !!!! recheck when to "restart"..
class rule_runner {
    legacy::ruleT m_rule;
    legacy::tileT m_tile, m_side;
    int m_gen = 0;

    friend class rule_recorder;
    void reset_rule(const legacy::ruleT& rule) {
        m_rule = rule;
        restart();
    }

public:
    // TODO: clarify ownership...
    tile_filler* m_filler = nullptr; // source.

    const legacy::ruleT& rule() const {
        return m_rule;
    }

    const legacy::tileT& tile() const {
        return m_tile;
    }

    int gen() const {
        return m_gen;
    }

    void restart() {
        m_filler->random_fill(m_tile);
        m_gen = 0;
    }

    void run(int count) {
        for (int i = 0; i < count; ++i) {
            m_tile.gather().apply(m_rule, m_side);
            m_tile.swap(m_side);
        }
        m_gen += count;
    }

    // TODO: support in gui.
    // TODO: should the filler be shifted too? if so then must be tile-based...
    void shift(int dy, int dx) {
        m_tile.shift(dy, dx, m_side);
        m_tile.swap(m_side);
    }

    rule_runner(legacy::shapeT shape) : m_tile(shape), m_side(shape, legacy::tileT::no_clear) {}
};

class rule_recorder {
    std::vector<legacy::compress> m_record;
    int m_pos = -1; // always<=size()-1.// TODO: how to make m_pos always valid?

    void invariants() const {
        if (!m_record.empty()) {
            assert(m_pos >= 0);
        }
        assert(m_pos >= 0 && m_pos < m_record.size());
    }

public:
    // TODO: init_state is problematic...
    // TODO: can this be null?
    // TODO: nullibility? ownership?
    rule_runner* m_runner = nullptr; // notify. // TODO: shared_ptr?
    rule_maker* m_maker = nullptr;   // source.
    // TODO: analyser... (notify...)

    // ????
    void attach_maker(rule_maker* maker) {
        m_maker = maker;
        new_rule();
    }

    bool empty() const {
        return m_record.empty();
    }

    int size() const {
        return m_record.size();
    }

    int pos() const {
        return m_pos;
    }

    // TODO: look for better names...
    void take(const legacy::ruleT& rule) {
        // TODO: refine logic, search around...
        // TODO: let history hold one rule to avoid awkwardness...
        // assert(m_runner->rule() == *pos); // TODO:redesign this function.
        if (m_record.empty() || m_runner->rule() != rule) {
            m_record.emplace_back(rule);
            m_pos = m_record.size() - 1;

            m_runner->reset_rule(rule);
        }
    }

    void new_rule() {
        if (m_maker) {
            take(m_maker->make());
        }
    }

    // TODO: random-access mode...
    // TODO: current...

    void next() {
        assert(m_pos + 1 <= m_record.size());
        if (m_pos + 1 != m_record.size()) {
            m_runner->reset_rule(legacy::ruleT(m_record[++m_pos]));
        } else {
            new_rule();
        }
    }

    void prev() {
        assert(m_pos != -1); // TODO: needed? enough?
        if (m_pos > 0) {
            m_runner->reset_rule(legacy::ruleT(m_record[--m_pos]));
        }
    }

    // TODO: ctor...maker-ctor as...
};
