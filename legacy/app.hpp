#pragma once

#include <random>
#include <unordered_map>

#include "partition.hpp"
#include "rule.hpp"
#include "serialize2.hpp"
#include "tile.hpp"

// TODO: partial rule (e.g. collect from tile; the rest is generated from another generator...)
// TODO: allow resizing the grid.
// TODO: rule editor... (based on mini-window, click the pixel to set...)..
// TODO: where to add gol?
// TODO: (important) small window...

// TODO: notify on quit...
// TODO!!!!!support more run-mode...
// TODO: some settings should leave states in paused state...
// TODO: right click to enable/disable miniwindow...
// TODO: file container. easy ways to add fav...

// TODO: really?
/* implicitly inline */ void random_fill(bool* begin, bool* end, int count, auto&& rand) {
    int dist = end - begin;
    assert(dist > 0);
    std::fill(begin, end, false);
    std::fill(begin, begin + std::clamp(count, 0, dist), true);
    std::shuffle(begin, end, rand);
}

// TODO: seed-only? (for easier reproduction...)
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

        random_fill(tile.begin(), tile.end(), tile.area() * density, std::mt19937_64{m_rand});
    }
};

// TODO: support shifting...
// TODO: !!!! recheck when to "restart"..
class rule_runner {
    legacy::ruleT m_rule{};
    legacy::tileT m_tile, m_side;
    int m_gen = 0;

    friend class rule_recorder;
    void reset_rule(const legacy::ruleT& rule) {
        m_rule = rule;
        restart(); // TODO: optionally start from current state...
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
            ++m_gen;
        }
    }

    // TODO: should the filler be shifted too? if so then must be tile-based...
    // TODO: recheck logic...
    void shift_xy(int dx, int dy) {
        dx = ((-dx % m_tile.width()) + m_tile.width()) % m_tile.width();
        dy = ((-dy % m_tile.height()) + m_tile.height()) % m_tile.height();
        for (int y = 0; y < m_tile.height(); ++y) {
            bool* source = m_tile.line((y + dy) % m_tile.height());
            bool* dest = m_side.line(y);
            std::copy_n(source, m_tile.width(), dest);
            std::rotate(dest, dest + dx, dest + m_tile.width());
        }

        m_tile.swap(m_side);
    }

    explicit rule_runner(legacy::rectT size) : m_tile(size), m_side(size) {}
};

// TODO: how to suitably deal with empty state?????
// TODO: support file...
// TODO: more obvious ways to record important rules...
class rule_recorder {
    std::vector<legacy::compressT> m_record;
    // std::unordered_map<legacy::compress, int> m_map;

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
    // TODO: next/prev work still poorly with generator, editor etc...
    void take(const legacy::ruleT& rule) {
        // TODO: requiring syntronized...
        if (m_record.empty() || m_runner->rule() != rule) {
            m_record.emplace_back(rule);
            m_pos = m_record.size() - 1;
            m_runner->reset_rule(rule);
        }
    }

    // TODO: current...

    // TODO: pos>=size() ->size()-1?
    bool set_pos(int pos) {
        if (pos >= 0 && pos < size()) {
            if (pos != m_pos) {
                m_runner->reset_rule(legacy::ruleT(m_record[m_pos = pos]));
            }
            return true;
        }
        return false;
    }

    bool next() {
        return set_pos(m_pos + 1);
    }

    bool prev() {
        return set_pos(m_pos - 1);
    }

    // TODO: reconsider m_pos logic...
    // TODO: append looks problematic with m_pos logic...
    void append(const std::vector<legacy::compressT>& vec) {
        m_record.insert(m_record.end(), vec.begin(), vec.end());
        if (!m_record.empty() && m_pos == -1) {
            m_runner->reset_rule(legacy::ruleT(m_record[m_pos = 0])); // TODO: why = 0?
        }
    }

    void replace(std::vector<legacy::compressT> vec) {
        if (!vec.empty()) {
            m_record.swap(vec);
            m_runner->reset_rule(legacy::ruleT(m_record[m_pos = 0]));
        }
        // else???
    }
};

std::vector<legacy::compressT> extract_rules(const char* begin, const char* end) {
    std::vector<legacy::compressT> rules;

    const auto& regex = legacy::regex_MAP_str();
    std::cmatch match;
    while (std::regex_search(begin, end, match, regex)) {
        legacy::compressT rule(legacy::from_MAP_str(match[0]));
        if (rules.empty() || rules.back() != rule) {
            rules.push_back(rule);
        }
        begin = match.suffix().first;
    }
    return rules;
}

// TODO: refine...
// TODO: forbid exception...
std::vector<legacy::compressT> read_rule_from_file(const char* filename) {
    if (FILE* fp = fopen(filename, "rb")) {
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        if (size > 1000000) {
            fclose(fp);
        } else {
            std::vector<char> data(size);
            fseek(fp, 0, SEEK_SET);
            int read = fread(data.data(), 1, size, fp);
            fclose(fp);
            assert(read == size); // TODO: how to deal with this?
            return extract_rules(data.data(), data.data() + data.size());
        }
    }
    return {};
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: should allow multiple record.
// "current_record" ~editor~modify-each...
// extract a rule and set as ...
// open file... merge multiple file into ...
// export as file...
// current-record???->which notify which?
// TODO: empty state?
