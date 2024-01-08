#pragma once

// TODO: the utilities provided by "app.hpp" appears unsystematic, and generally too weak.

#include <deque>
#include <format>
#include <optional>
#include <random>
#include <span>

#include "rule.hpp"
#include "tile.hpp"

// TODO: add the rationale...
static std::mt19937 global_mt19937(time(0));

// TODO: better name...
// TODO: explain why float (there is no instant ImGui::SliderDouble)
// (std::optional<uint32_t> has proven to be very awkward)
struct tileT_fill_arg {
    bool use_seed;
    uint32_t seed;
    float density; // ∈ [0.0f, 1.0f]
};

inline void random_fill(legacy::tileT& tile, const tileT_fill_arg& filler) {
    // TODO: is the algo correct?
    // TODO: explain why not using bernoulli_distribution
    const auto fill_tile = [&tile](double density, std::mt19937& rand) {
        const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
        for (bool& b : tile.data()) {
            b = rand() < c;
        }
    };

    if (filler.use_seed) {
        std::mt19937 rand(filler.seed);
        fill_tile(filler.density, rand);
    } else {
        fill_tile(filler.density, global_mt19937);
    }
}

#if 0
// Flip density...
// TODO: regional fill...
inline void random_fill_v2(legacy::tileT& tile, const legacy::tileT& period, const tileT_fill_arg& filler) {
    legacy::piece_up(period, tile);

    const auto fill_tile = [&tile](double density, std::mt19937& rand) {
        const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
        for (bool& b : tile.data()) {
            b ^= (rand() < c);
        }
    };

    if (filler.use_seed) {
        std::mt19937 rand(filler.seed);
        fill_tile(filler.density, rand);
    } else {
        fill_tile(filler.density, global_mt19937);
    }
}
#endif

class torusT {
    legacy::tileT m_tile, m_side;
    int m_gen;

public:
    explicit torusT(const legacy::rectT& size) : m_tile(size), m_side(size), m_gen(0) {}

    // TODO: reconsider whether to expose non-const tile...
    legacy::tileT& tile() { return m_tile; }
    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(tileT_fill_arg filler, std::optional<legacy::rectT> resize = {}) {
        if (resize) {
            m_tile.resize(*resize);
        }

        random_fill(m_tile, filler);
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

        // TODO: proper name...
        const auto round_clip = [](int v, int r) { return ((v % r) + r) % r; };
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
// TODO: (gui) whether to support random-access mode?
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

    void replace_current(const legacy::ruleT& rule) { //
        m_record[m_pos] = legacy::compressT(rule);
    }

    legacy::ruleT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return static_cast<legacy::ruleT>(m_record[m_pos]);
    }

    void set_pos(int pos) { //
        m_pos = std::clamp(pos, 0, size() - 1);
    }
    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

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
