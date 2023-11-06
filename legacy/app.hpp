#pragma once

#include <deque>
#include <format>
#include <random>

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

    // TODO: this turns out to be a mess...
    friend class rule_recorder;
    void reset_rule(const legacy::ruleT& rule) {
        m_rule = rule;
        restart(); // TODO: optionally start from current state...
    }

    // TODO: experimental... maybe not suitable for rule_runner.
    // The logic looks very fragile... reliance is a mess...
    // TODO: recheck logic...
    int init_shift_x = 0, init_shift_y = 0;

    void do_shift_xy(int dx, int dy) {
        const int width = m_tile.width(), height = m_tile.height();

        dx = ((-dx % width) + width) % width;
        dy = ((-dy % height) + height) % height;
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
        m_gen = 0;
        m_filler->fill(m_tile);
        do_shift_xy(init_shift_x, init_shift_y);
    }

    void run(int count) {
        for (int i = 0; i < count; ++i) {
            m_tile.gather().apply(m_rule, m_side);
            m_tile.swap(m_side);
            ++m_gen;
        }
    }

    void shift_xy(int dx, int dy) {
        init_shift_x += dx;
        init_shift_y += dy; // TODO: wrap eagerly...
        do_shift_xy(dx, dy);
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

    void set_pos(int pos) {
        assert(!empty());
        pos = std::clamp(pos, 0, size() - 1);
        if (pos != m_pos) {
            m_runner->reset_rule(legacy::ruleT(m_record[m_pos = pos]));
        }
    }

    void next() {
        set_pos(m_pos + 1);
    }

    void prev() {
        set_pos(m_pos - 1);
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

#include "save.hpp"
#include <imgui.h>

// Likely to be the only singleton...
class logger {
    // TODO: maybe timestamp-and-inheritance-based...
    std::deque<std::string> m_strs{};
    int m_max;
    int i = 0;

public:
    // TODO: m_max should be constrainted...
    explicit logger(int max_size = 20) : m_max(max_size) {}

    // Also serve as error handler; must succeed.
    template <class... T> // TODO: clang-format setting is problematic here...
    void log(std::format_string<const T&...> fmt, const T&... args) noexcept {
        auto now = legacy::timeT::now();
        // TODO: the format should be refined...
        char str[100];
        snprintf(str, 100, "%d-%d %02d:%02d:%02d [%d] ", now.month, now.day, now.hour, now.min, now.sec, i++);

        m_strs.push_back(str + std::format(fmt, args...));
        if (m_strs.size() > m_max) {
            m_strs.pop_front();
        }
    }

    // TODO: layout is terrible...
    void window(const char* id_str, bool* p_open = nullptr) {
        if (ImGui::Begin(id_str, p_open)) {
            int n_max = m_max;
            ImGui::PushItemWidth(200);
            if (ImGui::SliderInt("Max record [20,50]", &n_max, 20, 50)) {
                m_max = n_max;
                while (m_strs.size() > m_max) {
                    m_strs.pop_front();
                }
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_strs.clear();
            }

            ImGui::Separator();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGuiListClipper clipper;
            clipper.Begin(m_strs.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    ImGui::TextUnformatted(m_strs[i].c_str());
                }
            }
            clipper.End();
            ImGui::PopStyleVar();
        }
        ImGui::End();
    }
};
