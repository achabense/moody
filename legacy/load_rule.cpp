// TODO: not used in this source file...
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "app.hpp"

// - Experience in MSVC
// - It turns out that there are still a lot of messy encoding problems even if "/utf-8" is specified.
//   (For example, how is `exception.what()` encoded? What does `path` expects from `string`? And what about
//   `filesystem.path.string()`?)
static std::string cpp17_u8string(const std::filesystem::path& p) {
    return reinterpret_cast<const char*>(p.u8string().c_str());
}

// TODO: recheck...
// TODO: able to create/append/open file?
class file_nav {
    using path = std::filesystem::path;
    using entries = std::vector<std::filesystem::directory_entry>;
    using clock = std::chrono::steady_clock;

    char buf_path[200]{};
    char buf_filter[20]{".txt"};

    // TODO: is canonical path necessary?
    bool valid = false; // The last call to `collect(current, ...)` is successful.
    path current;       // Canonical path.
    entries dirs, files;

    clock::time_point expired = {}; // TODO: better name...

    static void collect(const path& p, entries& dirs, entries& files) {
        dirs.clear();
        files.clear();
        for (const auto& entry :
             std::filesystem::directory_iterator(p, std::filesystem::directory_options::skip_permission_denied)) {
            const auto status = entry.status();
            if (is_directory(status)) {
                dirs.emplace_back(entry);
            }
            if (is_regular_file(status)) {
                files.emplace_back(entry);
            }
        }
    }

    // Intentionally pass by value.
    void set_current(path p) {
        try {
            p = std::filesystem::canonical(p);
            entries p_dirs, p_files;
            collect(p, p_dirs, p_files);

            valid = true;
            current.swap(p);
            dirs.swap(p_dirs);
            files.swap(p_files);

            expired = clock::now() + 3000ms;
        } catch (const std::exception& what) {
            // TODO: report error... what encoding?
        }
    }

    void refresh_if_valid() {
        if (valid && clock::now() > expired) {
            try {
                collect(current, dirs, files);
                expired = clock::now() + 3000ms;
            } catch (const std::exception& what) {
                // TODO: report error... what encoding?
                valid = false;
                dirs.clear();
                files.clear();
            }
        }
    }

public:
    file_nav(path p = std::filesystem::current_path()) {
        valid = false;
        set_current(std::move(p));
    }

    // TODO: refine...
    inline static std::vector<std::pair<path, std::string>> additionals;
    static bool add_special_path(path p, const char* title) { //
        std::error_code ec{};
        p = std::filesystem::canonical(p, ec);
        if (!ec) {
            additionals.emplace_back(std::move(p), title);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::optional<path> display() {
        std::optional<path> target = std::nullopt;

        refresh_if_valid();

        if (valid) {
            imgui_str(cpp17_u8string(current));
        } else {
            assert(dirs.empty() && files.empty());
            imgui_strdisabled("(Invalid) " + cpp17_u8string(current));
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                // TODO: better hint...
                if (ImGui::InputTextWithHint("Path", "-> enter", buf_path, std::size(buf_path),
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    // TODO: is the usage of / correct?
                    // TODO: is this still ok when !valid?
                    set_current(current / std::filesystem::u8path(buf_path));
                    buf_path[0] = '\0';
                }
                for (const auto& [path, title] : additionals) {
                    if (ImGui::MenuItem(title.c_str())) {
                        set_current(path);
                    }
                }
                if (ImGui::MenuItem("..")) {
                    set_current(current.parent_path());
                }
                ImGui::Separator();
                if (auto child = imgui_childwindow("Folders")) {
                    if (dirs.empty()) {
                        imgui_strdisabled("None");
                    }
                    const std::filesystem::directory_entry* sel = nullptr;
                    for (const auto& entry : dirs) {
                        // TODO: cache str?
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            sel = &entry;
                        }
                    }
                    // TODO: explain this is the reason why `set_current` must take by value...
                    if (sel) {
                        // TODO: does directory_entry.path always return absolute path?
                        assert(sel->path().is_absolute());
                        set_current(sel->path());
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

bool file_nav_add_special_path(std::filesystem::path p, const char* title) {
    return file_nav::add_special_path(std::move(p), title);
}

#if 0
// TODO: path must be a regular file...
// TODO: fileT loading may be better of going through load_binary...
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
    logger::add_msg(300ms, "Cannot load");
    return {};
}
#endif

// TODO: should support clipboard paste in similar ways...
// TODO: support saving into file? (without relying on the clipboard)
// TODO: support in-memory loadings. (e.g. tutorial about typical ways to find interesting rules)

// TODO: refine...
struct fileT {
    static constexpr int null_id = -1;
    struct lineT {
        int id;
        std::string text;
    };

    std::filesystem::path m_path;
    std::vector<lineT> m_lines;
    std::vector<legacy::compressT> m_rules;
    int pointing_at = 0; // valid if !m_rules.empty().

    // TODO: whether to throw if not found?
    fileT(std::filesystem::path path) : m_path(std::move(path)) {
        std::ifstream ifs(m_path);
        std::string line;
        int id = 0;

        while (std::getline(ifs, line)) {
            auto extr = legacy::extract_rules(line);
            bool has_rule = !extr.empty();
            // (wontfix) intentionally not moving `line`, as otherwise MSVC will give a moved-from warning.
            m_lines.emplace_back(has_rule ? id++ : null_id, line);
            if (has_rule) {
                m_rules.push_back(extr[0]);
                // TODO: (how&) whether to support more fine-grained (in-line) rule-location?
                // (only the first rule in each line is being recognized... (so `extract_rules` is wasteful)
            }
        }

        assert(id == m_rules.size());
    }

    // TODO: how to combine with file-nav (into a single window)?
    std::optional<legacy::ruleT> display(const legacy::ruleT& test_sync) {
        static const bool wrap = true; // TODO: (temporarily const)

        std::optional<legacy::ruleT> out;
        bool hit = false; // TODO: also sync on window appearing?
        const int total = m_rules.size();

        if (total != 0) {
            const bool in_sync = test_sync == m_rules[pointing_at];

            iter_pair(
                "<|", "prev", "next", "|>",                                              //
                [&] { hit = true, pointing_at = 0; },                                    //
                [&] { hit = true, pointing_at = std::max(0, pointing_at - 1); },         //
                [&] { hit = true, pointing_at = std::min(total - 1, pointing_at + 1); }, //
                [&] { hit = true, pointing_at = total - 1; }, false, false);
            ImGui::SameLine();
            ImGui::Text("Total:%d At:%d %s", total, pointing_at + 1, !in_sync ? "(click to sync)" : "");
            // TODO: what if there are popups?
            if (!in_sync && ImGui::IsItemHovered()) {
                const ImVec2 pos_min = ImGui::GetItemRectMin();
                const ImVec2 pos_max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
            }

            if (ImGui::IsItemClicked()) {
                hit = true;
            }

            // TODO: (temp) without ImGuiFocusedFlags_ChildWindows, clicking the child window will invalidate this.
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                if (imgui_keypressed(ImGuiKey_UpArrow, true)) {
                    hit = true, pointing_at = std::max(0, pointing_at - 1);
                }
                if (imgui_keypressed(ImGuiKey_DownArrow, true)) {
                    hit = true, pointing_at = std::min(total - 1, pointing_at + 1);
                }
            }
        } else {
            ImGui::Text("Not found");
        }

        const bool focus = hit;

        ImGui::Separator();

        if (auto child = imgui_childwindow("Child", {}, 0,
                                           wrap ? ImGuiWindowFlags_None : ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            // TODO: refine line-no logic (line-no or id-no)?
            for (int l = 1; const auto& [id, text] : m_lines) {
                ImGui::TextDisabled("%2d ", l++);
                ImGui::SameLine();
                if (wrap) {
                    imgui_strwrapped(text);
                } else {
                    imgui_str(text);
                }
                if (id != null_id) {
                    if (id == pointing_at) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 60));
                        if (!ImGui::IsItemVisible() && focus) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            pointing_at = id;
                            hit = true;
                        }
                    }
                }
            }
            ImGui::PopStyleVar();
        }

        if (hit) {
            assert(pointing_at >= 0 && pointing_at < total);
            return m_rules[pointing_at];
        } else {
            return std::nullopt;
        }
    }
};

// TODO: reset scroll-y for new files...
// TODO: show the last opened file in file-nav?

std::optional<legacy::ruleT> load_rule(const legacy::ruleT& test_sync) {
    static file_nav nav;
    static std::optional<fileT> file;

    std::optional<legacy::ruleT> out;

    bool close = false;
    if (file) {
        // TODO: the path (& file_nav's) should be copyable...

        // TODO (temp) selectable looks good but is easy to click by mistake...
        if (ImGui::SmallButton("Close")) {
            close = true;
        }
        imgui_str(cpp17_u8string(file->m_path));

        out = file->display(test_sync);
    } else {
        if (auto sel = nav.display()) {
            file.emplace(*sel);
            if (file->m_rules.empty()) {
                file.reset();
                logger::add_msg(1000ms, "No rules"); // TODO: better msg...
            }
        }
    }

    if (close) {
        file.reset();
    }

    // TODO: whether to support opening multiple files?
    // TODO: better layout...

    return out;
}
