#include <deque>
#include <filesystem>
#include <fstream>
#include <ranges>

#include "common.hpp"

// TODO: whether to support write access (file-editing)?
// Or at least support saving rules into file (without relying on the clipboard)?

using pathT = std::filesystem::path;

// (wontfix) After wasting so much time, I'd rather afford the extra copy than bothering with "more efficient"
// implementations any more.

static std::string cpp17_u8string(const pathT& path) {
    const auto u8string = path.u8string();
    return std::string(u8string.begin(), u8string.end());
}

// As to why not using `filesystem::u8path`:
// There is no standard way to shut the compiler up for a [[deprecated]] warning.
// As to making an `std::u8string` first, see:
// https://stackoverflow.com/questions/57603013/how-to-safely-convert-const-char-to-const-char8-t-in-c20
// In short, in C++20 it's impossible to get `char8_t*` from `char*` without copy and in a well-defined way.
static pathT cpp17_u8path(const std::string_view path) { //
    return pathT(std::u8string(path.begin(), path.end()));
}

// I hate this part so much...
// (This is horribly inefficient, but there are not going to be too many calls in each frame, so let it go.)
[[nodiscard]] static std::string clip_path(const pathT& p, const float avail_w, bool* clipped = nullptr) {
    if (p.empty()) {
        clipped && (*clipped = false);
        return "";
    }

    std::string full_str = cpp17_u8string(p);
    const float full_w = ImGui::CalcTextSize(full_str.c_str()).x;
    if (full_w <= avail_w) {
        clipped && (*clipped = false);
        return full_str;
    } else {
        clipped && (*clipped = true);
        if (!p.has_relative_path()) {
            return full_str;
        }

        // Try to make a shorter string in the form of:
        // .../longest suffix in the relative_path within `avail_w`, always including the last element.
        std::vector<pathT> segs;
        for (pathT seg : p.relative_path()) {
            segs.push_back(std::move(seg));
        }
        assert(!segs.empty());

        // ~ `&sep + 1` is valid, see:
        // https://stackoverflow.com/questions/14505851/is-the-one-past-the-end-pointer-of-a-non-array-type-a-valid-concept-in-c
        const char sep = pathT::preferred_separator;
        const float sep_w = ImGui::CalcTextSize(&sep, &sep + 1).x;

        std::vector<std::string> vec;
        vec.push_back(cpp17_u8string(segs.back()));
        float suffix_w = ImGui::CalcTextSize(vec.back().c_str()).x + ImGui::CalcTextSize("...").x + sep_w;
        for (auto pos = segs.rbegin() + 1; pos != segs.rend(); ++pos) {
            std::string seg_str = cpp17_u8string(*pos);
            const float seg_w = ImGui::CalcTextSize(seg_str.c_str()).x;
            if (suffix_w + (seg_w + sep_w) <= avail_w) {
                suffix_w += (seg_w + sep_w);
                vec.push_back(std::move(seg_str));
            } else {
                break;
            }
        }

        if (suffix_w > full_w) {
            // This may happen in rare cases like `C:/very-long-name`.
            return full_str;
        } else {
            std::string str = "...";
            for (auto pos = vec.rbegin(); pos != vec.rend(); ++pos) {
                str += sep;
                str += *pos;
            }
            return str;
        }
    }
}

// (Sharing the same style as `imgui_StrCopyable`.)
static void display_path(const pathT& p, float avail_w) {
    bool clipped = false;
    imgui_Str(clip_path(p, avail_w, &clipped));
    if (clipped) {
        imgui_ItemTooltip([&] { imgui_Str(cpp17_u8string(p)); });
    }
    if (imgui_ItemClickable()) {
        ImGui::SetClipboardText(cpp17_u8string(p).c_str());
    }
}

static void display_filename(const pathT& p) {
    imgui_Str(std::string("...") + char(pathT::preferred_separator) + cpp17_u8string(p.filename()));
    imgui_ItemTooltip([&] { imgui_Str(cpp17_u8string(p)); });
    if (imgui_ItemClickable()) {
        ImGui::SetClipboardText(cpp17_u8string(p).c_str());
    }
}

static pathT home_path{};
bool set_home(const char* u8path) {
    auto try_set = [](const char* u8path) {
        std::error_code ec{};
        const pathT p = u8path ? cpp17_u8path(u8path) : std::filesystem::current_path(ec);
        if (!ec) {
            pathT cp = std::filesystem::canonical(p, ec);
            if (!ec) {
                // These will outlive the imgui context.
                static std::string ini_path, log_path;

                ini_path = cpp17_u8string(home_path / "imgui.ini");
                log_path = cpp17_u8string(home_path / "imgui_log.txt");
                ImGui::GetIO().IniFilename = ini_path.c_str();
                ImGui::GetIO().LogFilename = log_path.c_str();
                home_path.swap(cp);
                return true;
            }
        }
        return false;
    };

    return (u8path && try_set(u8path)) || try_set(nullptr);
}

class file_nav {
    using entryT = std::filesystem::directory_entry;

    char buf_path[200]{};
    char buf_filter[20]{};

    pathT m_current{};                       // Canonical path; empty() <-> invalid.
    std::vector<entryT> m_dirs{}, m_files{}; // invalid -> empty()
    std::deque<pathT> m_record{};            // Record for valid m_current; front() ~ most recent one.
    int max_record = 10;

    static void collect(const pathT& path, std::vector<entryT>& dirs, std::vector<entryT>& files) {
        dirs.clear();
        files.clear();
        for (const auto& entry :
             std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
            const auto status = entry.status();
            if (is_directory(status)) {
                dirs.emplace_back(entry);
            }
            if (is_regular_file(status)) {
                files.emplace_back(entry);
            }
        }
    }

    // (`path` may come from `entryT.path()` in `m_dirs`.)
    bool set_current(const pathT& path) {
        try {
            pathT p = std::filesystem::canonical(path);
            std::vector<entryT> p_dirs, p_files;
            collect(p, p_dirs, p_files);

            {
                // (`m_record` is small enough.)
                const auto find = std::find(m_record.begin(), m_record.end(), p);
                if (find != m_record.end()) {
                    m_record.erase(find);
                }
                m_record.push_front(p);
                if (m_record.size() > max_record) {
                    m_record.resize(max_record);
                }
            }

            m_current.swap(p);
            m_dirs.swap(p_dirs);
            m_files.swap(p_files);
            return true;
        } catch (const std::exception& /* not used; the encoding is a mystery */) {
            messenger::add_msg("Cannot open folder:\n{}", cpp17_u8string(path));
        }
        return false;
    }

public:
    void refresh_if_valid() {
        if (!m_current.empty()) {
            try {
                collect(m_current, m_dirs, m_files);
            } catch (const std::exception& /* not used; the encoding is a mystery */) {
                messenger::add_msg("Cannot refresh folder:\n{}", cpp17_u8string(m_current));
                m_current.clear();
                m_dirs.clear();
                m_files.clear();
            }
        }
    }

    file_nav() { set_current(home_path); }

    void select_file(const pathT* current_file /* Optional */, std::optional<pathT>& target) {
        ImGui::SetNextItemWidth(std::min(ImGui::CalcItemWidth(), (float)item_width));
        ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
        ImGui::Separator();
        if (auto child = imgui_ChildWindow("Files")) {
            bool has = false;
            for (const entryT& entry : m_files) {
                const std::string str = cpp17_u8string(entry.path().filename());
                if (str.find(buf_filter) != str.npos) {
                    has = true;
                    const bool selected = current_file && *current_file == entry.path();
                    // (`Selectable` will not close the popup from child-window.)
                    if (ImGui::Selectable(str.c_str(), selected)) {
                        target = entry.path();
                    }
                    if (selected && ImGui::IsWindowAppearing()) {
                        ImGui::SetScrollHereY();
                    }
                }
            }
            if (!has) {
                imgui_StrDisabled("None");
            }
        }
    }

    void select_history() {
        ImGui::SetNextItemWidth(item_width);
        imgui_StepSliderInt("Limit", &max_record, 5, 15);
        imgui_Str("Recently opened folders");
        if (m_record.size() > max_record) {
            m_record.resize(max_record);
        }
        ImGui::Separator();
        if (auto child = imgui_ChildWindow("Records")) {
            if (m_record.empty()) {
                imgui_StrDisabled("None");
            }
            const pathT* sel = nullptr;
            for (bool f = true; pathT & p : m_record) {
                const bool is_current = std::exchange(f, false) && p == m_current;
                if (ImGui::Selectable(clip_path(p, ImGui::GetContentRegionAvail().x).c_str(), is_current)) {
                    sel = &p;
                }
            }
            if (sel) {
                set_current(*sel);
            }
        }
    }

    void show_current() {
        if (!m_current.empty()) {
            display_path(m_current, ImGui::GetContentRegionAvail().x);
        } else {
            assert(m_dirs.empty() && m_files.empty());
            imgui_StrDisabled("N/A");
        }
    }

    // Return one of `entryT.path()` in `m_files`.
    std::optional<pathT> display() {
        std::optional<pathT> target = std::nullopt;

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                ImGui::SetNextItemWidth(std::min(ImGui::CalcItemWidth(), (float)item_width));
                const bool enter = ImGui::InputTextWithHint("##Path", "Folder or file path", buf_path,
                                                            std::size(buf_path), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                if ((ImGui::Button("Open") || enter) && buf_path[0] != '\0') {
                    const bool succ = [&]() -> bool {
                        std::error_code ec{};
                        const pathT p = m_current / cpp17_u8path(buf_path);
                        if (std::filesystem::is_directory(p, ec)) {
                            return set_current(p);
                        } else if (std::filesystem::is_regular_file(p, ec) && set_current(p.parent_path())) {
                            // (wontfix) Inefficient, but there are a lot of uncertainties about `entryT.path()`.
                            // (Are there better ways to find such an entry? Is `entryT.path()` canonical?
                            // Does `target = canonical(p)` or `target = entryT(p).path()` work?)
                            for (const entryT& entry : m_files) {
                                if (std::filesystem::equivalent(entry.path(), p, ec)) {
                                    target = entry.path();
                                    return true;
                                }
                            }
                        }
                        return false;
                    }();
                    if (!succ) {
                        messenger::add_msg("Cannot open path:\n{}", buf_path);
                    }

                    buf_path[0] = '\0';
                }
                ImGui::Separator();
                // (Using `ImGuiSelectableFlags_NoPadWithHalfSpacing` for the same visual effect as
                // those in _ChildWindow("Folders").)
                if (!home_path.empty()) {
                    if (ImGui::Selectable("Home", false, ImGuiSelectableFlags_NoPadWithHalfSpacing)) {
                        set_current(home_path);
                    }
                }
                if (ImGui::Selectable("..", false, ImGuiSelectableFlags_NoPadWithHalfSpacing)) {
                    set_current(m_current.parent_path());
                }
                ImGui::Separator();
                if (auto child = imgui_ChildWindow("Folders")) {
                    if (m_dirs.empty()) {
                        imgui_StrDisabled("None");
                    }
                    const entryT* sel = nullptr;
                    for (const entryT& entry : m_dirs) {
                        // TODO: cache str?
                        const std::string str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            sel = &entry;
                        }
                    }
                    if (sel) {
                        set_current(sel->path());
                    }
                }
            }
            ImGui::TableNextColumn();
            select_file(nullptr, target);
            ImGui::EndTable();
        }

        return target;
    }
};

// It is easy to locate all rules in the text via `extract_MAP_str`.
// However there are no easy ways to locate or highlight (only) the rule across the lines.
// See: https://github.com/ocornut/imgui/issues/2313
// So, currently `textT` is line-based, and only recognizes the first rule for each line, and will
// highlight the whole line if the line contains a rule.
class textT {
    struct lineT {
        std::string text;
        std::optional<int> id = std::nullopt;
    };

    std::vector<lineT> m_lines{};
    std::vector<aniso::extrT::valT> m_rules{};
    std::optional<int> m_pos = std::nullopt; // `display` returned m_rules[*m_pos] last time.

    struct selT {
        int beg = 0, end = 0;
        bool contains(int l) const {
            if (beg < end) {
                return beg <= l && l <= end;
            } else {
                return end <= l && l <= beg;
            }
        }
        std::pair<int, int> minmax() const { return std::minmax(beg, end); }
    };
    std::optional<selT> m_sel = std::nullopt;

public:
    textT() {}
    textT(std::string_view str) { append(str); }

    bool has_rule() const { return !m_rules.empty(); }

    void clear() {
        m_lines.clear();
        m_rules.clear();
        m_pos.reset();
        m_sel.reset();
    }

    // `str` is assumed to be utf8-encoded.
    // (If not, the rules are still likely extractable.)
    void append(std::string_view str) {
        for (const auto& l : std::views::split(str, '\n')) {
            lineT& line = m_lines.emplace_back(std::string(l.data(), l.size()));
            const auto val = aniso::extract_MAP_str(l).val;
            if (val.has_value()) {
                m_rules.push_back(*val);
                line.id = m_rules.size() - 1;
            }
        }
    }

    // TODO: refactor... the logics have been a mess...
    // (Workaround: using `rewind` tag to reset the scroll after opening new files.)
    void display(std::optional<aniso::extrT::valT>& out, bool rewind = false) {
        if (m_sel && ImGui::IsMouseReleased(ImGuiMouseButton_Right) /* From anywhere */) {
            std::string str;
            const auto [min, max] = m_sel->minmax();
            for (int i = min; i <= max; ++i) {
                if (i != min) {
                    str += '\n';
                }
                str += m_lines[i].text;
            }

            // The problem (see the "Workaround" part below) can happen as long as wrap-pos is enabled.
            // For example, push-wrap-pos -> ImGui::Text will result in the same issue.
            ImGui::SetClipboardText(str.c_str());
            m_sel.reset();
#if 0
            // TODO: whether to silently call `ImGui::SetClipboardText`?
            messenger::add_msg(str);
            m_sel.reset();
            // (wontfix) The message popup will appear at next frame, so it's still possible that an extra click
            // is made (and begin selection) at this frame. As a result, when entering the popup the right-button
            // may still be held down, and if released it will bring anther string to the popup.
#endif
        }

        // TODO: whether to share the same config across the windows?
        static bool preview_mode = true;
        static previewer::configT config(previewer::configT::_220_160);

        bool ret = false;
        const int total = m_rules.size();

        if (total != 0) {
            std::optional<int> n_pos = std::nullopt;
            if (ImGui::Button("Focus")) {
                n_pos = m_pos.value_or(0);
            }
            ImGui::SameLine();
            sequence::seq(
                "<|", "Prev", "Next", "|>",                                   //
                [&] { n_pos = 0; },                                           //
                [&] { n_pos = std::max(0, m_pos.value_or(-1) - 1); },         //
                [&] { n_pos = std::min(total - 1, m_pos.value_or(-1) + 1); }, //
                [&] { n_pos = total - 1; });
            ImGui::SameLine();
            if (m_pos.has_value()) {
                ImGui::Text("Total:%d At:%d", total, *m_pos + 1);
            } else {
                ImGui::Text("Total:%d At:N/A", total);
            }

            if (!m_sel && n_pos) {
                assert(*n_pos >= 0 && *n_pos < total);
                m_pos = *n_pos;
                ret = true;
            }

            ImGui::Checkbox("Preview mode", &preview_mode);
            if (preview_mode) {
                ImGui::SameLine();
                config.set("Settings", "Restart");
            }
        } else {
            ImGui::Text("(No rules)");
        }

        const bool locate = ret;
        ImGui::Separator();

        if (rewind) {
            ImGui::SetNextWindowScroll({0, 0});
        }
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 24, 24, 255));
        if (auto child = imgui_ChildWindow("Child")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 0; const auto& [text, id] : m_lines) {
                const int this_l = l++;
                ImGui::TextDisabled("%2d ", this_l + 1);
                ImGui::SameLine();
                if (preview_mode && id.has_value()) {
                    ImGui::BeginGroup();
                }
                {
                    // (Workaround: `ImGui::TextWrapped` cannot smartly render long single-lines.)
                    // (Related: https://github.com/ocornut/imgui/issues/5720)
                    if (text.size() < 2000) {
                        imgui_StrWrapped(text, item_width);
                    } else {
                        ImGui::BeginGroup();
                        const char* begin = text.data();
                        const char* const end = text.data() + text.size();
                        while (end - begin > 1000) {
                            const char* seg_end = begin + 1000; // seg_end < end
                            // Find a meaningful (utf8) code-point end nearby.
                            for (int i = 0; i < 4; ++i) {
                                if ((*seg_end & 0b11'000000) != 0b10'000000) {
                                    break;
                                }
                                --seg_end;
                            }
                            imgui_StrWrapped(std::string_view(begin, seg_end), item_width);
                            begin = seg_end;
                        }
                        if (begin != end) {
                            imgui_StrWrapped(std::string_view(begin, end), item_width);
                        }
                        ImGui::EndGroup();
                    }
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    m_sel = {this_l, this_l};
                } else if (m_sel && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    m_sel->end = this_l;
                }
                if (m_sel && m_sel->contains(this_l)) {
                    imgui_ItemRectFilled(IM_COL32(255, 255, 255, 90));
                }

                if (id.has_value()) {
                    // Pretend there is no lock if `!manage_lock::enabled()`.
                    const bool has_lock = manage_lock::enabled() && m_rules[*id].lock.has_value();

                    if (*id == m_pos) {
                        imgui_ItemRectFilled(IM_COL32(has_lock ? 196 : 0, 255, 0, 60));
                    }
                    if (!m_sel && ImGui::IsItemHovered()) {
                        imgui_ItemRectFilled(IM_COL32(has_lock ? 196 : 0, 255, 0, 30));
                        if (!locate && ImGui::IsItemClicked()) {
                            m_pos = *id;
                            ret = true;
                        }
                    }

                    if (preview_mode) {
                        imgui_StrDisabled("-: ");
                        ImGui::SameLine();
                        if (*id != 0 && m_rules[*id].rule == m_rules[*id - 1].rule) {
                            imgui_StrDisabled("The same as the last rule.");
                        } else {
                            previewer::preview(*id, config, m_rules[*id].rule, true);
                        }
                        ImGui::EndGroup();
                    }

                    // It's ok to test fully-visible even if the region is not large enough.
                    if (locate && *id == m_pos && !imgui_ItemFullyVisible()) {
                        ImGui::SetScrollHereY();
                    }
                }
            }
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor();

        if (ret) {
            assert(m_pos && *m_pos >= 0 && *m_pos < total);
            // (Relying on `sequence::seq` to be in the same level in the id-stack.)
            sequence::bind_to(ImGui::GetID("Next"));
            out = m_rules[*m_pos];
        }
    }
};

static const int max_size = 1024 * 256;

// For error message.
static std::string too_long(uintmax_t size, int max_size) {
    const bool use_mb = size >= 1024 * 1024;
    return std::format("{:.2f}{} > {:.2f}KB", size / (use_mb ? (1024 * 1024.0) : 1024.0), use_mb ? "MB" : "KB",
                       max_size / 1024.0);
}

static std::string load_binary(const pathT& path) {
    std::error_code ec{};
    const auto size = std::filesystem::file_size(path, ec);
    if (!ec && size <= max_size) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (file) {
            std::string data(size, '\0');
            file.read(data.data(), size);
            if (file && file.gcount() == size) {
                return data;
            }
        }
    }

    if (!ec && size > max_size) {
        messenger::add_msg("File too large: {}\n{}", too_long(size, max_size), cpp17_u8string(path));
    } else {
        messenger::add_msg("Failed to load file:\n{}", cpp17_u8string(path));
    }
    throw 0;
}

// TODO: support opening multiple files?
// TODO: add a mode to avoid opening files without rules?
static void load_rule_from_file(std::optional<aniso::extrT::valT>& out) {
    static file_nav nav;

    static textT text;
    static bool rewind = false;
    static std::optional<pathT> path;

    auto try_load = [](const pathT& p) -> bool {
        try {
            text = textT(load_binary(p));
            return true;
        } catch (...) {
            return false;
        }
    };

    if (!path) {
        if (ImGui::SmallButton("Refresh")) {
            nav.refresh_if_valid();
        }
        ImGui::SameLine();
        ImGui::SmallButton("...");
        // `BeginPopupContextItem` will consume the settings, even if it is not opened.
        ImGui::SetNextWindowSize({0, 200}, ImGuiCond_Always);
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            nav.select_history();
            ImGui::EndPopup();
        }
        nav.show_current();

        ImGui::Separator();
        if (auto sel = nav.display(); sel && try_load(*sel)) {
            rewind = true;
            path = std::move(*sel);
        }
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) {
            try_load(*path);
            // Don't rewind.
        }
        ImGui::SameLine();
        ImGui::SmallButton("...");
        ImGui::SetNextWindowSize({0, 200}, ImGuiCond_Always);
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            // TODO: the popup may be closed unexpectedly by messenger's popup if the load fails.
            // (I haven't figured out how to make the two popups display together...)
            std::optional<pathT> sel = std::nullopt;
            nav.select_file(&*path, sel);
            if (sel && try_load(*sel)) {
                rewind = true; // Rewind, even if the new path is the same as the old one.
                path = std::move(*sel);
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        display_filename(*path);

        ImGui::Separator();
        text.display(out, std::exchange(rewind, false));
        if (close) {
            path.reset();
            text.clear();
        }
    }
}

static void load_rule_from_clipboard(std::optional<aniso::extrT::valT>& out) {
    static textT text;
    static bool rewind = false;
    if (ImGui::SmallButton("Read clipboard")) {
        if (const char* str = ImGui::GetClipboardText()) {
            std::string_view str_view(str);
            if (str_view.size() > max_size) {
                messenger::add_msg("Text too long: {}", too_long(str_view.size(), max_size));
            } else {
                text.clear();
                text.append(str_view);
                rewind = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        text.clear();
    }

    ImGui::Separator();
    text.display(out, std::exchange(rewind, false));
}

// Defined in "docs.cpp". [0]:title [1]:contents, null-terminated.
extern const char* const docs[][2];

static void load_rule_from_memory(std::optional<aniso::extrT::valT>& out) {
    static textT text;
    static bool rewind = false;
    static std::optional<int> doc_id = std::nullopt;
    static auto select = []() {
        for (int i = 0; docs[i][0] != nullptr; ++i) {
            const auto [title, contents] = docs[i];
            if (ImGui::Selectable(title, doc_id == i, ImGuiSelectableFlags_DontClosePopups) && doc_id != i) {
                text.clear();
                text.append(contents);
                rewind = true;
                doc_id = i;
            }
        }
    };

    if (!doc_id) {
        imgui_Str("Astral (v 0.9-beta) by github user 'achabense'.");
        imgui_Str("The latest version is available at: ");
        ImGui::SameLine(0, 0);
        imgui_StrCopyable("https://github.com/achabense/astral", imgui_Str);

        ImGui::Separator();
        select();
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        ImGui::SmallButton("...");
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            select();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        imgui_Str(docs[*doc_id][0]);

        ImGui::Separator();
        text.display(out, std::exchange(rewind, false));
        if (close) {
            text.clear();
            doc_id.reset();
        }
    }
}

// Preserving the `load_rule_from_xx` functions to allow for integration.
std::optional<aniso::extrT::valT> load_file() {
    std::optional<aniso::extrT::valT> out = std::nullopt;
    load_rule_from_file(out);
    return out;
}

std::optional<aniso::extrT::valT> load_clipboard() {
    std::optional<aniso::extrT::valT> out = std::nullopt;
    load_rule_from_clipboard(out);
    return out;
}

std::optional<aniso::extrT::valT> load_doc() {
    std::optional<aniso::extrT::valT> out = std::nullopt;
    load_rule_from_memory(out);
    return out;
}
