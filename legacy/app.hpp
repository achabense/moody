#pragma once

#include <random>
#include <unordered_map>

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
// TODO: rnd-mode for tile...(stable vs arbitrary...)
// TODO: right click to enable/disable miniwindow...
// TODO: fine-grained rule edition...
// TODO: file container. easy ways to add fav...

// TODO: really?
/* implicitly inline */ void random_fill(bool* begin, bool* end, int count, auto&& rand) {
    int dist = end - begin;
    assert(dist > 0);
    std::fill(begin, end, false);
    std::fill(begin, begin + std::clamp(count, 0, dist), true);
    std::shuffle(begin, end, rand);
}

class tile_filler {
    std::mt19937_64 m_rand;

public:
    explicit tile_filler(uint64_t seed) : m_rand{seed} {}

    float density = 0.5; // ¡Ê [0.0f, 1.0f]

    void disturb() {
        // Enough to make totally different result:
        (void)m_rand();
    }
    void fill(legacy::tileT& tile, const legacy::rectT* resize = nullptr) const {
        if (resize) {
            tile.resize(*resize);
        }

        int width = tile.width(), height = tile.height();
        int area = width * height;
        bool* data = new bool[area];
        random_fill(data, data + area, area * density, std::mt19937_64{m_rand});

        for (int y = 0; y < height; ++y) {
            memcpy(tile.line(y), data + y * width, width);
        }

        delete[] data;
    }
};

// TODO: support inc,dec, next_perm, prev_perm...
// ugh, horrible, this seems belong to editor... (as maker seems just a partition generator...)
// TODO: density is actually for generation, not matching with cursor now...
// TODO: editor, very hard...
// TODO: how to support user-defined partition?
// TODO: what's the relation with analyzer/ editor?
// TODO: how to interact with recorder automatically?
class rule_maker {
    std::mt19937_64 m_rand;

public:
    // TODO: "density" might not be suitable name...
    // density ¡Ê [0, max_density()]
    int density = 0; // TODO: rename. TODO: revert density setting?

    // TODO: aside from filters, the problem is, [what] to support?
    // TODO: partially symmetric rules?
    static constexpr const char* mode_names[]{"none(deprecated)", "sub_spatial",   "sub_spatial2", "spatial",
                                              "spatial_paired",   "spatial_state", "permutation"};
    enum generator : int {
        none, // arbitrary 512 str, not used.
        sub_spatial,
        sub_spatial2,
        spatial,
        spatial_paired,
        spatial_state,
        perm
    } g_mode = spatial; // TODO: how to control this?

    // TODO: explain what this is exactly...
    // TODO: the enum is problematic...
    legacy::interpret_mode interpret_as = legacy::ABS;

    explicit rule_maker(uint64_t seed) : m_rand{seed} {
        density = max_density() * 0.3;
    }

    const legacy::partitionT& current_partition() const {
        std::underlying_type_t<legacy::interpret_mode>;
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
        legacy::ruleT_base grule{}; // TODO: is it safe not to do value init?
        random_fill(grule.data(), grule.data() + max_density(), density, m_rand);
        legacy::ruleT_base rule = current_partition().dispatch_from(grule);
        return legacy::ruleT(rule, interpret_as);
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
        m_filler->fill(m_tile);
        m_gen = 0;
    }

    void run(int count) {
        for (int i = 0; i < count; ++i) {
            m_tile.gather().apply(m_rule, m_side);
            m_tile.swap(m_side);
        }
        m_gen += count;
    }

    // TODO: support "shift"; should the filler be shifted too? if so then must be tile-based...

    rule_runner(legacy::rectT size) : m_tile(size), m_side(size) {}
};

// TODO: how to suitably deal with empty state?????
class rule_recorder {
    std::vector<legacy::compress> m_record;
    std::unordered_map<legacy::compress, int> m_map;

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
    // TODO: nullability? ownership?
    rule_runner* m_runner = nullptr; // notify. // TODO: shared_ptr?
    // TODO: analyzer... (notify...)

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
    // TODO: reconsider what should be done when already exists...
    void take(const legacy::ruleT& rule) {
        legacy::compress cmpr(rule);
        auto find = m_map.find(cmpr);
        if (find != m_map.end()) {
            m_pos = find->second;
        } else {
            m_record.push_back(cmpr);
            m_pos = m_record.size() - 1;
            m_map.emplace(cmpr, m_pos);
        }

        // TODO: this is awkward...
        if (m_runner->rule() != rule) {
            m_runner->reset_rule(rule);
        }
    }

    // TODO: random-access mode...
    // TODO: current...

    void next() {
        assert(m_pos + 1 <= m_record.size());
        if (m_pos + 1 != m_record.size()) {
            m_runner->reset_rule(legacy::ruleT(m_record[++m_pos]));
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

//////////////////////////////////////////////////////////////////////////////////////////////////////
// in developement...

// TODO: use this instead...
class tile_runner {
    legacy::tileT m_tile, m_side;
    bool initialized = false;

public:
    tile_runner(legacy::rectT size) : m_tile(size), m_side(size) {}

    void reset(const tile_filler& filler, const legacy::rectT* resize = nullptr) {
        filler.fill(m_tile, resize);
        initialized = true;
    }

    const legacy::tileT& tile() const {
        assert(initialized);
        return m_tile;
    }

    void run(const legacy::ruleT& rule) {
        assert(initialized);
        m_tile.gather().apply(rule, m_side);
        m_tile.swap(m_side);
    }
};

struct scannerT {
    const legacy::partitionT* const partition;
    std::array<bool, 512> grule;
    bool matches;

    void scan(const legacy::ruleT_base& rule, bool force) {
        matches = partition->matches(rule) || force; // TODO: "|| force" suitable?
        if (matches) {
            partition->gather_from(rule);
        }
    }
};

class rule_editor {
    std::mt19937_64 m_rand;

public:
    std::vector<scannerT> scanlist; // must not be empty...
    int using_partition = 0;

    std::array<bool, 512> m_grule; // grouped by using-partition.
    const legacy::partitionT* m_partition;

    bool matches = false;
    legacy::ruleT_base m_rulebase;
    int interpret_as = legacy::ABS; // TODO: the enum is problematic...

    legacy::ruleT to_rule() const {
        return legacy::ruleT(m_rulebase, (legacy::interpret_mode)interpret_as);
    }

    // TODO: the cost will never be significant.
    void take_rule(const legacy::ruleT& rule) {
        m_rulebase = rule.to_base((legacy::interpret_mode)interpret_as);
        for (auto& scanner : scanlist) {
            scanner.scan(rule, false);
        }
    }

    // TODO: support masking mode?
    enum update_mode {
        randomized,
        addition,
        permutation,
    } m_mode;

    void update() {
        switch (m_mode) {}
    }

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

    explicit rule_editor(uint64_t seed) : m_rand{seed} {
        density = max_density() * 0.3;
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
        random_fill(grule.data(), grule.data() + max_density(), density, m_rand);
        legacy::ruleT::array_base rule = current_partition().dispatch_from(grule);
        return legacy::ruleT(rule, (legacy::interpret_mode)interpret_as);
    }
};
