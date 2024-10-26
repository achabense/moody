#include <deque>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <unordered_map>

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
    auto set_clipped = [&clipped](bool b) {
        if (clipped) {
            *clipped = b;
        }
    };
    if (p.empty()) {
        set_clipped(false);
        return "";
    }

    std::string full_str = cpp17_u8string(p);
    const float full_w = ImGui::CalcTextSize(full_str.c_str()).x;
    if (full_w <= avail_w) {
        set_clipped(false);
        return full_str;
    } else {
        set_clipped(true);
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
    if (imgui_ItemClickableSingle()) {
        set_clipboard_and_notify(cpp17_u8string(p));
    }
    guide_mode::item_tooltip(clipped ? "\n(Right-click to copy.)" : "Right-click to copy.");
}

static void display_filename(const pathT& p) {
    imgui_Str(std::string("...") + char(pathT::preferred_separator) + cpp17_u8string(p.filename()));
    imgui_ItemTooltip([&] { imgui_Str(cpp17_u8string(p)); });
    if (imgui_ItemClickableSingle()) {
        set_clipboard_and_notify(cpp17_u8string(p));
    }
    guide_mode::item_tooltip("\n(Right-click to copy.)");
}

static pathT home_path{};
bool set_home(const char* u8path) {
    auto try_set = [](const char* u8path) {
        std::error_code ec{};
        const pathT p = u8path ? cpp17_u8path(u8path) : std::filesystem::current_path(ec);
        if (!ec) {
            pathT cp = std::filesystem::canonical(p, ec);
            if (!ec) {
                home_path.swap(cp);

#if 0
                // These will outlive the imgui context.
                static std::string ini_path, log_path;
                ini_path = cpp17_u8string(home_path / "imgui.ini");
                log_path = cpp17_u8string(home_path / "imgui_log.txt");
                ImGui::GetIO().IniFilename = ini_path.c_str();
                ImGui::GetIO().LogFilename = log_path.c_str();
#endif
                return true;
            }
        }
        return false;
    };

    return (u8path && try_set(u8path)) || try_set(nullptr);
}

// TODO: redesign error message...
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
            messenger::set_msg("Cannot open folder:\n{}", cpp17_u8string(path));
        }
        return false;
    }

public:
    void refresh_if_valid() {
        if (!m_current.empty()) {
            try {
                collect(m_current, m_dirs, m_files);
            } catch (const std::exception& /* not used; the encoding is a mystery */) {
                messenger::set_msg("Cannot refresh folder:\n{}", cpp17_u8string(m_current));
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
                    // if (ImGui::Selectable(str.c_str(), selected)) {
                    if (imgui_SelectableStyledButton(str.c_str(), selected)) {
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

    // TODO: redesign, and consider supporting recently-opened files as well.
    void select_history() {
        // ImGui::SetNextItemWidth(item_width);
        // imgui_StepSliderInt("Limit", &max_record, 5, 15);
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
            for (bool f = true; const pathT& p : m_record) {
                const bool is_current = std::exchange(f, false) && p == m_current;
                // if (ImGui::Selectable(clip_path(p, ImGui::GetContentRegionAvail().x).c_str(), is_current)) {
                if (imgui_SelectableStyledButton(clip_path(p, ImGui::GetContentRegionAvail().x).c_str(), is_current)) {
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
            imgui_LockTableLayoutWithMinColumnWidth(140); // TODO: improve...
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                ImGui::SetNextItemWidth(std::min(ImGui::CalcItemWidth(), (float)item_width));
                if (ImGui::InputTextWithHint("Open", "Folder or file path", buf_path, std::size(buf_path),
                                             ImGuiInputTextFlags_EnterReturnsTrue) &&
                    buf_path[0] != '\0') {
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
                        messenger::set_msg("Cannot open path:\n{}", buf_path);
                    }

                    buf_path[0] = '\0';
                }
                ImGui::Separator();
                // TODO: instead of hiding this entry, should explicitly notify it's not available.
                if (!home_path.empty()) {
                    // (Using `ImGuiSelectableFlags_NoPadWithHalfSpacing` for the same visual effect as
                    // those in _ChildWindow("Folders").)
                    // if (ImGui::Selectable("Home", false, ImGuiSelectableFlags_NoPadWithHalfSpacing)) {
                    if (imgui_SelectableStyledButton("Home")) {
                        set_current(home_path);
                    }
                }
                // if (ImGui::Selectable("..", false, ImGuiSelectableFlags_NoPadWithHalfSpacing)) {
                if (imgui_SelectableStyledButton("..")) {
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
                        // if (ImGui::Selectable(str.c_str())) {
                        if (imgui_SelectableStyledButton(str.c_str())) {
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

struct preview_setting {
    bool enabled = true;
    previewer::configT config = previewer::configT::_220_160;
};

// It is easy to locate all rules in the text via `extract_MAP_str`.
// However there are no easy ways to locate or highlight (only) the rule across the lines.
// See: https://github.com/ocornut/imgui/issues/2313
// So, currently `textT` is line-based, and only recognizes the first rule for each line, and will
// highlight the whole line if the line contains a rule.
class textT {
    std::string m_text{};
    std::vector<aniso::compressT> m_rules{};

    // Won't be invalidated by reallocation.
    struct str_ref {
        int begin = 0, size = 0;
        std::string_view get(const std::string& str) const { //
            return {str.data() + begin, (size_t)size};
        }
    };
    struct rule_ref {
        int pos = -1;
        bool has_value() const { return pos != -1; }
        const aniso::compressT& get(const std::vector<aniso::compressT>& rules) const {
            assert(pos >= 0 && pos < (int)rules.size());
            return rules[pos];
        }
    };
    struct line_ref {
        str_ref str = {};   // -> `m_text`
        rule_ref rule = {}; // -> `m_rules`
        bool eq_last = false;
    };

    std::vector<line_ref> m_lines{};

    line_ref& _append_line(const std::string_view line) {
        const str_ref ref = {(int)m_text.size(), (int)line.size()};
        if (!line.empty()) {
            m_text += line;
        }
        return m_lines.emplace_back(ref);
    }
    void _attach_rule(line_ref& line, const aniso::ruleT& rule) {
        m_rules.emplace_back(rule);
        const int pos = m_rules.size() - 1;
        line.rule.pos = pos;
        line.eq_last = pos > 0 ? m_rules[pos] == m_rules[pos - 1] : false;
    }

    std::optional<int> m_pos = std::nullopt; // `display` returned m_rules[*m_pos] last time.

    struct selT {
        int beg = 0, end = 0; // []
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

    preview_setting m_preview{}; // TODO: move out of class?

    bool rewind = false;

public:
    textT() {}
    textT(std::string_view str) { append(str); }

    textT& operator=(const textT&) = delete;

    int count_rule() const { return m_rules.size(); }

    void clear() {
        m_lines.clear();
        m_rules.clear();
        m_pos.reset();
        m_sel.reset();
    }

    // `str` is assumed to be utf8-encoded.
    // (If not, the rules are still likely extractable.)
    void append(const std::string_view str) {
        for (const auto& l : std::views::split(str, '\n')) {
            line_ref& line = _append_line({l.data(), l.size()});
            if (const auto extr = aniso::extract_MAP_str(l); extr.has_rule()) {
                _attach_rule(line, extr.get_rule());
            }
        }
    }

    void append(const aniso::ruleT& rule) {
        line_ref& line = _append_line(aniso::to_MAP_str(rule));
        _attach_rule(line, rule);
    }

    void reset_scroll() { rewind = true; }

    void display(sync_point& out, const std::optional<int> o_pos = std::nullopt, preview_setting* o_preview = nullptr) {
        if (m_sel) {
            if (ImGui::IsWindowAppearing()) {
                // This should not happen, as the interaction to the parent window will be blocked
                // when there are selected lines.
                assert(false);
                m_sel.reset(); // Defensive.
            } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) /* From anywhere */) {
                m_sel.reset();
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) /* From anywhere */ ||
                       shortcuts::test(ImGuiKey_C) /*Raw test; the interaction will be locked by `m_sel`*/) {
                const auto [min, max] = m_sel->minmax();
                std::string str;
                for (int i = min; i <= max; ++i) {
                    if (i != min) {
                        str += '\n';
                    }
                    str += m_lines[i].str.get(m_text);
                }
                // (wontfix) `SetClipboardText` will skip contents after '\0' (normally a utf8 text file
                // should not contain '\0' between the lines).
                set_clipboard_and_notify(str);
            }
        }

        {
            preview_setting& n_preview = o_preview ? *o_preview : m_preview;
            std::optional<int> n_pos = std::nullopt;
            if (o_pos && !m_rules.empty()) {
                n_pos = std::clamp(*o_pos, 0, int(m_rules.size()));
            }

            // Precedence:
            // Line-selecting > locating > (starting line-selection) > left-click setting
            if (const auto pos = display_header(n_preview); !n_pos) {
                n_pos = pos;
            }
            ImGui::Separator();
            if (std::exchange(rewind, false)) {
                ImGui::SetNextWindowScroll({0, 0});
            }
            if (const auto [pos, sel] = display_page(!m_sel ? n_pos : std::nullopt, n_preview); pos || sel) {
                if (!n_pos) {
                    n_pos = pos;
                }
                if (sel) {
                    m_sel = sel;
                }
            }

            if (!m_sel && n_pos) {
                assert(*n_pos >= 0 && *n_pos < int(m_rules.size()));
                out.set(m_rules[*n_pos]);
                m_pos = *n_pos;
            }
        }

        // Prevent interaction with other widgets, so that for example, the parent window cannot be
        // closed when there are selected lines.
        if (m_sel) {
            const ImGuiID claim = ImGui::GetID("Claim");
            ImGuiWindow* const window = ImGui::GetCurrentWindow();

            // (Idk which are actually necessary/preferable, but the following combination works as intended.)

            // ImGui::SetHoveredID(claim);
            ImGui::SetActiveID(claim, window);
            // ImGui::SetFocusID(claim, window);
            ImGui::FocusWindow(window);

            // Otherwise, for some reason the parent window will still be collapsed if its
            // title bar is double-clicked.
            // Related: https://github.com/ocornut/imgui/issues/7841
            ImGui::SetKeyOwner(ImGui::MouseButtonToKey(ImGuiMouseButton_Left), claim);
            // ImGui::SetKeyOwner(ImGui::MouseButtonToKey(ImGuiMouseButton_Right), claim);
        }
    }

private:
    [[nodiscard]] std::optional<int> display_header(preview_setting& n_preview) const {
        std::optional<int> n_pos = std::nullopt;

        const int total = m_rules.size();
        if (total != 0) {
            // ImGui::BeginGroup();
            sequence::seq(
                "<|", "Prev", "Next", "|>",                                   //
                [&] { n_pos = 0; },                                           //
                [&] { n_pos = std::max(0, m_pos.value_or(-1) - 1); },         //
                [&] { n_pos = std::min(total - 1, m_pos.value_or(-1) + 1); }, //
                [&] { n_pos = total - 1; });
            // ImGui::EndGroup();
            // imgui_ItemTooltip_StrID = "Seq##Rules";
            // guide_mode::item_tooltip("Rules found in the text.");

            ImGui::SameLine();
            if (m_pos.has_value()) {
                ImGui::Text("Total:%d At:%d", total, *m_pos + 1);
            } else {
                ImGui::Text("Total:%d At:N/A", total);
            }
            if (imgui_ItemClickableDouble()) {
                n_pos = m_pos.value_or(0);
            }
            imgui_ItemTooltip_StrID = "Sync";
            guide_mode::item_tooltip("Double right-click to move to 'At'.");

            ImGui::SameLine();
            ImGui::Checkbox("Preview", &n_preview.enabled);
            if (n_preview.enabled) {
                ImGui::SameLine();
                n_preview.config.set("Settings");
            }
        } else {
            imgui_Str("(No rules)");
        }

        return n_pos;
    }

    struct passT {
        std::optional<int> n_pos = std::nullopt;
        std::optional<selT> n_sel = std::nullopt;
    };

    [[nodiscard]] passT display_page(const std::optional<int> locate, const preview_setting& n_preview) const {
        assert_implies(m_sel, !locate);
        passT pass{};

        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_GREY(24, 255));
        if (auto child = imgui_ChildWindow("Content")) {
            set_scroll_by_up_down(ImGui::GetTextLineHeight() * 2); // TODO: document the behavior.

            const bool test_hover = (ImGui::IsWindowHovered() || m_sel) && ImGui::IsMousePosValid();
            const ImVec2 mouse_pos = ImGui::GetMousePos();
            const float region_max_x = imgui_ContentRegionMaxAbsX();
            ImDrawList* const drawlist = ImGui::GetWindowDrawList();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 0; const auto& [str, rule, eq_last] : m_lines) {
                const int this_l = l++;
                ImGui::TextDisabled("%2d ", this_l + 1);
                ImGui::SameLine();
                if (n_preview.enabled && rule.has_value()) {
                    ImGui::BeginGroup();
                }
                // (`ImGui::TextWrapped` has no problem rendering long single-lines now.)
                // (Related: https://github.com/ocornut/imgui/issues/7496)
                imgui_StrWrapped(str.get(m_text), item_width);
                const auto [str_min, str_max] = imgui_GetItemRect();
                const bool line_hovered = test_hover && mouse_pos.y >= str_min.y && mouse_pos.y < str_max.y;
                // `line_hovered` may become true for two adjacent lines if using `mouse_pos.y <= str_max.y`.

                if (!locate && line_hovered) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        pass.n_sel = {this_l, this_l};
                    } else if (m_sel && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        pass.n_sel = {m_sel->beg, this_l};
                    }
                }
                if (m_sel && m_sel->contains(this_l)) {
                    drawlist->AddRectFilled(str_min, {region_max_x, str_max.y}, IM_COL32_GREY(255, 16));
                    drawlist->AddRectFilled(str_min, str_max, IM_COL32_GREY(255, 40));
                }

                if (rule.has_value()) {
                    // TODO: temporarily preserved.
                    constexpr bool has_lock = false;

                    if (rule.pos == m_pos) {
                        drawlist->AddRectFilled(str_min, str_max, IM_COL32(has_lock ? 196 : 0, 255, 0, 60));
                    }
                    if (!m_sel &&
                        (line_hovered && mouse_pos.x >= str_min.x && mouse_pos.x < str_max.x /*str-hovered*/)) {
                        drawlist->AddRectFilled(str_min, str_max, IM_COL32(has_lock ? 196 : 0, 255, 0, 30));
                        if (!locate && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            pass.n_pos = rule.pos;
                        }
                    }

                    if (n_preview.enabled) {
                        imgui_StrDisabled("-: ");
                        ImGui::SameLine();
                        if (eq_last) {
                            imgui_StrDisabled("The same as the last rule.");
                        } else {
                            previewer::preview(rule.pos, n_preview.config, [&] { return rule.get(m_rules); });
                        }
                        ImGui::EndGroup();
                    }

                    // It's ok to test fully-visible even if the region is not large enough.
                    if (rule.pos == locate && !imgui_ItemFullyVisible()) {
                        ImGui::SetScrollHereY();
                    }
                }
            }
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor();

        assert_implies(locate, !pass.n_pos && !pass.n_sel);
        return pass;
    }
};

static const int max_size = 1024 * 256;

// For error message.
static std::string to_size(uintmax_t size) {
    const bool use_mb = size >= 1024 * 1024;
    return std::format("{:.2f}{}", size / (use_mb ? (1024 * 1024.0) : 1024.0), use_mb ? "MB" : "KB");
}

[[nodiscard]] static bool load_binary(const pathT& path, std::string& str) {
    std::error_code ec{};
    const auto size = std::filesystem::file_size(path, ec);
    if (!ec && size <= max_size) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (file) {
            std::string data(size, '\0');
            file.read(data.data(), size);
            if (file && file.gcount() == size) {
                str.swap(data);
                return true;
            }
        }
    }

    if (!ec && size > max_size) {
        messenger::set_msg("File too large: {} > {}", to_size(size), to_size(max_size));
    } else {
        messenger::set_msg("Failed to load file.");
    }
    return false;
}

// TODO: support opening multiple files?
// TODO: add a mode to avoid opening files without rules?
void load_file(sync_point& out) {
    static file_nav nav;

    static textT text;
    static std::optional<pathT> path;

    auto try_load = [](const pathT& p) -> bool {
        if (std::string str; load_binary(p, str)) {
            text.clear();
            text.append(str);
            return true;
        }
        return false;
    };

    if (!path) {
        if (ImGui::SmallButton("Refresh")) {
            nav.refresh_if_valid();
        }
        ImGui::SameLine();
        ImGui::SmallButton("Recent");
        // `BeginPopup` will consume the settings, even if not opened.
        ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Always);
        if (begin_popup_for_item()) {
            nav.select_history();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        nav.show_current();

        ImGui::Separator();
        if (auto sel = nav.display(); sel && try_load(*sel)) {
            text.reset_scroll();
            path = std::move(*sel);
        }
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) {
            try_load(*path);
            // Won't reset scroll.
        }
        ImGui::SameLine();
        ImGui::SmallButton("Select");
        ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Always);
        if (begin_popup_for_item()) {
            std::optional<pathT> sel = std::nullopt;
            nav.select_file(&*path, sel);
            if (sel && try_load(*sel)) {
                text.reset_scroll(); // Even if the new path is the same as the old one.
                path = std::move(*sel);
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        display_filename(*path);

        ImGui::Separator();
        text.display(out);
        if (close) {
            path.reset();
            text.clear();
        }
    }
}

void load_clipboard(sync_point& out) {
    static textT text;
    if (ImGui::SmallButton("Read clipboard") || shortcuts::item_shortcut(ImGuiKey_W)) {
        const std::string_view str = read_clipboard();
        if (str.size() > max_size) {
            messenger::set_msg("Text too long: {} > {}", to_size(str.size()), to_size(max_size));
        } else if (!str.empty()) {
            text.clear();
            text.append(str);
            text.reset_scroll();
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        text.clear();
    }

    ImGui::Separator();
    text.display(out);
}

// Defined in "docs.cpp". [0]:title [1]:contents, null-terminated.
extern const char* const docs[][2];

void load_doc(sync_point& out) {
    static textT text;
    static std::optional<int> doc_id = std::nullopt;
    static auto select = []() {
        for (int i = 0; docs[i][0] != nullptr; ++i) {
            const auto [title, contents] = docs[i];
            // if (ImGui::Selectable(title, doc_id == i, ImGuiSelectableFlags_NoAutoClosePopups) && doc_id != i) {
            if (imgui_SelectableStyledButton(title, doc_id == i) && doc_id != i) {
                text.clear();
                text.append(contents);
                text.reset_scroll();
                doc_id = i;
            }
        }
    };

    if (!doc_id) {
        imgui_Str("A toy for exploring MAP rules, by GitHub user 'achabense'.");
        imgui_Str("The latest version is available at: ");
        ImGui::SameLine(0, 0);
        // ImGui::TextLinkOpenURL("https://github.com/achabense/moody");
        imgui_StrCopyable("https://github.com/achabense/moody", imgui_Str, set_clipboard_and_notify);
        guide_mode::item_tooltip("Right-click to copy.");

        ImGui::Separator();
        select();
    } else {
        const bool close = ImGui::SmallButton("Close");
        ImGui::SameLine();
        ImGui::SmallButton("Select");
        if (begin_popup_for_item()) {
            select();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        imgui_Str(docs[*doc_id][0]);

        ImGui::Separator();
        text.display(out);
        if (close) {
            text.clear();
            doc_id.reset();
        }
    }
}

struct recordT {
    using mapT = std::unordered_map<aniso::compressT, int, aniso::compressT::hashT>;

    mapT m_map;
    textT m_text;

    void append(const aniso::ruleT& rule) {
        assert(m_map.size() == m_text.count_rule());
        if (m_map.try_emplace(rule, m_text.count_rule() /*`line.id`*/).second) {
            m_text.append(rule);
        }
    }

    std::optional<int> find(const aniso::ruleT& rule) {
        if (const auto fnd = m_map.find(rule); fnd != m_map.end()) {
            return fnd->second;
        }
        return std::nullopt;
    }

    void clear() {
        m_map.clear();
        m_text.clear();
    }
};

static recordT record_tested;
static recordT record_copied;

void rule_record::tested(const aniso::ruleT& rule) { //
    record_tested.append(rule);
}

void rule_record::copied(const aniso::ruleT& rule) { //
    record_copied.append(rule);
}

void rule_record::load_record(sync_point& sync) {
    static recordT* active = &record_tested;
    static preview_setting preview{.enabled = false};

    std::optional<int> locate = std::nullopt;
    if (ImGui::SmallButton("Clear")) { // !!TODO: whether to require double-click here?
        active->clear();
        if (active == &record_tested) {
            active->append(sync.rule);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Locate")) {
        locate = active->find(sync.rule);
        if (!locate.has_value()) {
            messenger::set_msg("Not found.");
        }
    }
    guide_mode::item_tooltip("..."); // !!TODO

    ImGui::SameLine();
    imgui_Str("Viewing ~");
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});
    imgui_RadioButton("Tested rules", &active, &record_tested);
    ImGui::SameLine();
    imgui_RadioButton("Copied rules", &active, &record_copied);
    ImGui::PopStyleVar();
    ImGui::SameLine();
    imgui_StrTooltip("(?)", "..."); // !!TODO
    guide_mode::highlight();

    ImGui::Separator();
    active->m_text.display(sync, locate, &preview);
}
