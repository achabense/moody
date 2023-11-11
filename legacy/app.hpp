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
// TODO: (important) small window...

// TODO: notify on quit...
// TODO: support more run-mode...
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

    float density = 0.5; // ∈ [0.0f, 1.0f]

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
    explicit rule_runner(legacy::rectT size) : m_tile(size), m_side(size) {}

    const legacy::ruleT& rule() const { return m_rule; }
    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(const tile_filler& filler) {
        m_gen = 0;
        filler.fill(m_tile);
        do_shift_xy(init_shift_x, init_shift_y);
    }

    // TODO: clumspy
    bool set_rule(const legacy::ruleT& rule) {
        if (m_rule != rule) {
            m_rule = rule;
            return true;
        }
        return false;
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
};

// Never empty.
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

    legacy::ruleT current() const { //
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

    void replace(std::vector<legacy::compressT> vec) {
        if (!vec.empty()) {
            m_record.swap(vec);
            m_pos = 0;
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

// It's unfortunate that `expected` is not in C++20...
std::optional<std::vector<char>> load_binary(const char* filename, int max_size) {
    std::unique_ptr<FILE, decltype(+fclose)> file(fopen(filename, "rb"), fclose);
    if (file) {
        FILE* fp = file.get();
        fseek(fp, 0, SEEK_END); // <-- what if fails?
        int size = ftell(fp);
        if (size < max_size) {
            fseek(fp, 0, SEEK_SET); // <-- what if fails?
            std::vector<char> data(size);
            fread(data.data(), 1, size, fp); // what if fails?
            return data;
        }
    }
    return {};
}

// TODO: also optional?
std::vector<legacy::compressT> read_rule_from_file(const char* filename) {
    auto result = load_binary(filename, 1'000'000);
    if (result) {
        return extract_rules(result->data(), result->data() + result->size());
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

// TODO: forbid copying...
struct [[nodiscard]] imgui_window {
    const bool visible;
    explicit imgui_window(const char* name, bool* p_open = {}, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};
// TODO: imgui_childwindow???

// Likely to be the only singleton...
class logger {
    // TODO: maybe better if data(inheritance)-based?
    static inline std::deque<std::string> m_strs{};
    static constexpr int m_max = 40;
    // ~ "ith" must not be a static in log(), as it's a template...
    static inline int ith = 0;

public:
    logger() = delete;

    // Also serve as error handler; must succeed.
    template <class... T>
    static void log(std::format_string<const T&...> fmt, const T&... args) noexcept {
        auto now = legacy::timeT::now();
        // TODO: the format should be refined...
        char str[100];
        snprintf(str, 100, "%d-%d %02d:%02d:%02d [%d] ", now.month, now.day, now.hour, now.min, now.sec, ith++);

        m_strs.push_back(str + std::format(fmt, args...));
        if (m_strs.size() > m_max) {
            m_strs.pop_front();
        }
    }

    // TODO: layout is terrible...
    static void window(const char* id_str, bool* p_open = nullptr) {
        if (imgui_window window(id_str, p_open); window) {
            if (ImGui::Button("Clear")) {
                m_strs.clear();
            }
            ImGui::SameLine();
            bool to_bottom = ImGui::Button("Bottom");
            ImGui::Separator();

            ImGui::BeginChild("Child");
#if 0
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGuiListClipper clipper;
            clipper.Begin(m_strs.size());
            // TODO: problematic...
            while (clipper.Step()) {
                // TODO: safe?
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    ImGui::TextUnformatted(m_strs[i].c_str());
                }
            }
            clipper.End();
            ImGui::PopStyleVar();
#else
            std::string str;
            for (const auto& s : m_strs) {
                str += s;
                str += '\n';
            }
            ImGui::TextWrapped(str.c_str());
#endif
            if (to_bottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
    }
};
