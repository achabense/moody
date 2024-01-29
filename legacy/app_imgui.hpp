#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>

#include "imgui.h"

#include "imgui_internal.h" // TODO: record dependency...

using namespace std::chrono_literals;

// Not necessary?
#if 0
// TODO: it seems explicit u8 encoding guarantee is not strictly needed in this project?
// - Assert that ordinary string literals are encoded with utf-8.
// - u8"..." is not used in this project, as it becomes `char8_t[]` after C++20 (which is not usable).
// - TODO: document ways to pass this check (/utf-8 etc; different compilers)...
inline void assert_utf8_encoding() {
    constexpr auto a = std::to_array("中文");
    constexpr auto b = std::to_array(u8"中文");

    static_assert(std::equal(a.begin(), a.end(), b.begin(), b.end(), [](auto l, auto r) {
        return static_cast<unsigned char>(l) == static_cast<unsigned char>(r);
    }));
}
#endif

// TODO: move elsewhere...
// - Experience in MSVC
// - It turns out that there are still a lot of messy encoding problems even if "/utf-8" is specified.
//   (For example, how is `exception.what()` encoded? What does `path` expects from `string`? And what about
//   `filesystem.path.string()`?)
inline std::string cpp17_u8string(const std::filesystem::path& p) {
    return reinterpret_cast<const char*>(p.u8string().c_str());
}

// Unlike ImGui::TextWrapped, doesn't take fmt str...
// TODO: the name is awful...
inline void imgui_str(std::string_view str) { //
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
}

// TODO: whether to apply std::format here?
// template <class... U>
// inline void imgui_strfmt(std::format_string<const U&...> fmt, const U&... args) {
//     imgui_str(std::format(fmt, args...));
// }

inline void imgui_strwrapped(std::string_view str) {
    ImGui::PushTextWrapPos(0.0f);
    imgui_str(str);
    ImGui::PopTextWrapPos();
}

inline void imgui_strcolored(const ImVec4& col, std::string_view str) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    imgui_str(str);
    ImGui::PopStyleColor();
}

inline void imgui_strdisabled(std::string_view str) {
    imgui_strcolored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], str);
}

// ~ referring to ImGui::InputScalar; recheck...
// TODO: better name; allow/disallow ++/-- by mouse scrolling?
inline bool imgui_int_slider(const char* label, int* v, int v_min, int v_max) {
    if (ImGui::GetCurrentWindow()->SkipItems) {
        return false;
    }

    int v2 = *v;

    const float r = ImGui::GetFrameHeight();
    const float s = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - 2 * (r + s)));
    ImGui::SliderInt("", &v2, v_min, v_max, "%d", ImGuiSliderFlags_NoInput);
    ImGui::PushButtonRepeat(true);
    ImGui::SameLine(0, s);
    // TODO: whether to push&pop framepadding?
    if (ImGui::Button("-", ImVec2(r, r))) {
        --v2;
    }
    ImGui::SameLine(0, s);
    if (ImGui::Button("+", ImVec2(r, r))) {
        ++v2;
    }
    ImGui::PopButtonRepeat();
    // TODO: FindRenderedTextEnd is a function in imgui_internal.h...
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label != label_end) {
        ImGui::SameLine(0, s);
        imgui_str(std::string_view(label, label_end));
    }
    ImGui::PopID();
    ImGui::EndGroup();

    v2 = std::clamp(v2, v_min, v_max);
    const bool changed = *v != v2;
    *v = v2;
    return changed;
}

inline bool imgui_keypressed(ImGuiKey key, bool repeat) {
    return !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(key, repeat);
};

// TODO: other mouse functions...
// TODO: rename to _mouse_XXX?
inline bool imgui_scrolling() { return ImGui::GetIO().MouseWheel != 0; }

inline bool imgui_scrolldown() { return ImGui::GetIO().MouseWheel < 0; }

inline bool imgui_scrollup() { return ImGui::GetIO().MouseWheel > 0; }

struct [[nodiscard]] imgui_window {
    imgui_window(const imgui_window&) = delete;
    imgui_window& operator=(const imgui_window&) = delete;

    const bool visible;
    // TODO: refine interface and documentation
    explicit imgui_window(const char* name, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, nullptr, flags)) {}
    explicit imgui_window(const char* name, bool* p_open, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_childwindow {
    imgui_childwindow(const imgui_childwindow&) = delete;
    imgui_childwindow& operator=(const imgui_childwindow&) = delete;

    const bool visible;
    explicit imgui_childwindow(const char* name, const ImVec2& size = {}, ImGuiChildFlags child_flags = {},
                               ImGuiWindowFlags window_flags = {})
        : visible(ImGui::BeginChild(name, size, child_flags, window_flags)) {}
    ~imgui_childwindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

class [[nodiscard]] imgui_itemtooltip {
    bool begun = false;

public:
    imgui_itemtooltip(const imgui_itemtooltip&) = delete;
    imgui_itemtooltip& operator=(const imgui_itemtooltip&) = delete;

    explicit imgui_itemtooltip(bool& toggle) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                toggle = !toggle;
            }
            if (toggle) {
                ImGui::BeginTooltip();
                begun = true;
            }
        }
    }
    ~imgui_itemtooltip() {
        if (begun) {
            ImGui::EndTooltip();
        }
    }

    explicit operator bool() const { return begun; }
};
#if 0
struct timeT {
    int year;
    int month; // [1,12]
    int day;   // [1,31]
    int hour;  // [0,23]
    int min;   // [0,59]
    int sec;   // [0,59]
    int ms;

    static timeT now() {
        using namespace std::chrono;
        auto now = current_zone()->to_local(system_clock::now());

        year_month_day ymd(floor<days>(now));
        int year = ymd.year().operator int();
        int month = ymd.month().operator unsigned int();
        int day = ymd.day().operator unsigned int();

        hh_mm_ss hms(floor<milliseconds>(now - floor<days>(now)));
        int hour = hms.hours().count();
        int min = hms.minutes().count();
        int sec = hms.seconds().count();
        int ms = hms.subseconds().count();
        return timeT{year, month, day, hour, min, sec, ms};
    }
};
#endif

class logger {
    // TODO: refine...
    struct temp_str {
        using clock = std::chrono::steady_clock;
        std::string str;
        clock::time_point deadline;

        temp_str(std::string&& str, std::chrono::milliseconds ms) : str(std::move(str)), deadline(clock::now() + ms) {}

        // TODO: better be expired(now=clock::now) return now>=deadline;
        bool expired() const { return clock::now() >= deadline; }
    };

    static inline std::vector<temp_str> m_tempstrs{};

public:
    logger() = delete;

    // TODO: replace XXXms with variables... (or enums...)
    template <class... U>
    static void log_temp(std::chrono::milliseconds ms, std::format_string<const U&...> fmt, const U&... args) noexcept {
        m_tempstrs.emplace_back(std::format(fmt, args...), ms);
    }

    // TODO: this might combine with itemtooltip...
    static void tempwindow() {
        if (!m_tempstrs.empty()) {
            ImGui::BeginTooltip();
            auto pos = m_tempstrs.begin();
            for (auto& temp : m_tempstrs) {
                imgui_str(temp.str);
                if (!temp.expired()) {
                    *pos++ = std::move(temp);
                }
            }
            m_tempstrs.erase(pos, m_tempstrs.end());
            ImGui::EndTooltip();
        }
    }
};

// TODO: refine...
// TODO: able to create/append/open file?
class file_nav {
    using path = std::filesystem::path;
    using clock = std::chrono::steady_clock;

    char buf_path[200]{};
    char buf_filter[20]{".txt"};

    std::vector<std::filesystem::directory_entry> dirs;
    std::vector<std::filesystem::directory_entry> files;

    clock::time_point expired = {};

    void set_current(const path& p) {
        // (Catching eagerly to avoid some flickering...)
        try {
            std::filesystem::current_path(p);
            expired = {};
        } catch (const std::exception& what) {
            // TODO: what encoding?
            // TODO: treat exceptions specially... (btw, 1000ms is too short)
            logger::log_temp(1000ms, "Exception:\n{}", what.what());
        }
    }
    void refresh() {
        // Setting outside of try-block to avoid too frequent failures...
        // TODO: log_temp is global; can be problematic...
        expired = std::chrono::steady_clock::now() + 3000ms;
        try {
            dirs.clear();
            files.clear();
            for (const auto& entry : std::filesystem::directory_iterator(
                     std::filesystem::current_path(), std::filesystem::directory_options::skip_permission_denied)) {
                const auto status = entry.status();
                if (is_regular_file(status)) {
                    files.emplace_back(entry);
                }
                if (is_directory(status)) {
                    dirs.emplace_back(entry);
                }
            }
        } catch (const std::exception& what) {
            // TODO: what encoding?
            logger::log_temp(1000ms, "Exception:\n{}", what.what());
        }
    }

public:
    // TODO: refine...
    inline static std::vector<std::pair<path, std::string>> additionals;
    static void add_special_path(path&& path, const char* title) { //
        additionals.emplace_back(std::move(path), title);
    }

    [[nodiscard]] std::optional<path> display() {
        std::optional<path> target = std::nullopt;

        imgui_strwrapped(cpp17_u8string(std::filesystem::current_path()));
        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            if (clock::now() > expired) {
                refresh();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                // TODO: better hint...
                if (ImGui::InputTextWithHint("Path", "-> enter", buf_path, std::size(buf_path),
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    set_current(std::filesystem::u8path(buf_path));
                    buf_path[0] = '\0';
                }
                for (const auto& [path, title] : additionals) {
                    if (ImGui::MenuItem(title.c_str())) {
                        set_current(path);
                    }
                }
                if (ImGui::MenuItem("..")) {
                    set_current("..");
                }
                ImGui::Separator();
                if (auto child = imgui_childwindow("Folders")) {
                    if (dirs.empty()) {
                        imgui_strdisabled("None");
                    }
                    for (const auto& entry : dirs) {
                        // TODO: cache str?
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            // Won't affect the loop at this frame.
                            set_current(entry.path());
                        }
                    }
                }
            }
            ImGui::TableNextColumn();
            {
                ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
                ImGui::Separator();
                if (auto child = imgui_childwindow("Files")) {
                    bool has = false;
                    for (const auto& entry : files) {
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (str.find(buf_filter) != str.npos) {
                            has = true;
                            if (ImGui::Selectable(str.c_str())) {
                                target = entry.path();
                            }
                        }
                    }
                    if (!has) {
                        imgui_strdisabled("None");
                    }
                }
            }
            ImGui::EndTable();
        }

        return target;
    }
};

// TODO: move eleswhere?
// TODO: explain why using filesystem::path...
inline std::vector<char> load_binary(const std::filesystem::path& path, int max_size) {
    std::error_code ec{};
    const auto size = std::filesystem::file_size(path, ec);
    if (size != -1 && size < max_size) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (file) {
            std::vector<char> data(size);
            file.read(data.data(), size);
            if (file && file.gcount() == size) {
                return data;
            }
        }
    }
    // TODO: refine msg?
    logger::log_temp(300ms, "Cannot load");
    return {};
}
