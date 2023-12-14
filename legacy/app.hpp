#pragma once

// TODO: the utilities provided by "app.hpp" appears unsystematic, and generally too weak.

#include <deque>
#include <format>
#include <random>
#include <span>

#include "rule.hpp"
#include "tile.hpp"

// TODO: allow resizing the grid.

// TODO: is the algo correct?
// TODO: explain count's impl-specific behavior...
inline void random_fill_density(bool* begin, bool* end, double density, std::mt19937& rand) {
    const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
    while (begin < end) {
        *begin++ = rand() < c;
    }
}

inline void random_fill_count(bool* begin, bool* end, int count, std::mt19937& rand) {
    std::fill(begin, end, false);
    std::fill(begin, begin + std::clamp(count, 0, int(end - begin)), true);
    std::shuffle(begin, end, rand);
}

// TODO: rename...
// TODO: explain why not double...
struct tile_filler {
    uint32_t seed; // arbitrary value
    float density; // ∈ [0.0f, 1.0f]
};

class torusT {
    legacy::tileT m_tile, m_side;
    int m_gen;

    // TODO: proper name...
    static int round_clip(int v, int r) {
        assert(r > 0);
        return ((v % r) + r) % r;
    }

public:
    explicit torusT(legacy::rectT size) : m_tile(size), m_side(size), m_gen(0) {}

    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    // by value or by cref? (also in tileT)
    void restart(tile_filler filler, std::optional<legacy::rectT> resize = {}) {
        if (resize) {
            m_tile.resize(*resize);
        }

        // TODO: count or density?
        std::mt19937 rand{filler.seed};
        random_fill_density(m_tile.begin(), m_tile.end(), filler.density, rand);

        m_gen = 0;
    }

    void run(const legacy::ruleT& rule, int count = 1) {
        for (int c = 0; c < count; ++c) {
            m_tile.gather(m_tile, m_tile, m_tile, m_tile, m_tile, m_tile, m_tile, m_tile);
            m_tile.apply(rule, m_side);
            m_tile.swap(m_side);

            ++m_gen;
        }
    }

    // TODO: recheck logic...
    void shift(int dx, int dy) {
        const int width = m_tile.width(), height = m_tile.height();

        dx = round_clip(-dx, width);
        dy = round_clip(-dy, height);
        if (dx == 0 && dy == 0) {
            return;
        }

        m_side.resize(m_tile.size());
        for (int y = 0; y < height; ++y) {
            bool* source = m_tile.line((y + dy) % height);
            bool* dest = m_side.line(y);
            std::copy_n(source, width, dest);
            std::rotate(dest, dest + dx, dest + width);
        }
        m_tile.swap(m_side);
    }
};

// Never empty.
// TODO: should be able to tell different sources...
class rule_recorder {
    std::vector<legacy::compressT> m_record;
    int m_pos;

public:
    rule_recorder() {
        m_record.emplace_back(legacy::game_of_life());
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    // TODO: look for better names...
    // TODO: reconsider what should be done when already exists in the whole vec...
    // TODO: next/prev work still poorly with editor...
    void take(const legacy::ruleT& rule) {
        legacy::compressT cmpr(rule);
        if (cmpr != m_record[m_pos]) {
            m_record.push_back(cmpr);
            m_pos = m_record.size() - 1;
        }
    }

    legacy::ruleT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return static_cast<legacy::ruleT>(m_record[m_pos]);
    }

    void set_pos(int pos) { //
        m_pos = std::clamp(pos, 0, size() - 1);
    }
    void next() { set_pos(m_pos + 1); }
    void prev() { set_pos(m_pos - 1); }

    // TODO: reconsider m_pos logic...
    void append(const std::vector<legacy::compressT>& vec) { //
        m_record.insert(m_record.end(), vec.begin(), vec.end());
    }

    void replace(std::vector<legacy::compressT>&& vec) {
        if (!vec.empty()) {
            m_record.swap(vec);
            m_pos = 0;
        }
        // else???
    }
};

// TODO: together with these, move to/from_MAP_str to a standalone header?
inline std::vector<legacy::compressT> extract_rules(const char* begin, const char* end) {
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

inline std::vector<legacy::compressT> extract_rules(std::span<const char> data) {
    return extract_rules(data.data(), data.data() + data.size());
}

inline std::vector<legacy::compressT> extract_rules(const char* str) {
    return extract_rules(str, str + strlen(str));
}
