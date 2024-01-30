#include "app.hpp"

// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.
// TODO: Generalize typical behavior patterns to find new rules.

void show_target_rule(const legacy::ruleT& target, rule_recorder& recorder) {
    const std::string rule_str = to_MAP_str(target);

    ImGui::AlignTextToFramePadding();
    imgui_str("[Current rule]");
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        ImGui::SetClipboardText(rule_str.c_str());
        logger::log_temp(300ms, "Copied");
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        // TODO: redesign... (especially, should not replace directly?)
        if (const char* text = ImGui::GetClipboardText()) {
            auto result = legacy::extract_rules(text);
            // TODO: copied from the main function...
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                recorder.replace(std::move(result));
            } else {
                logger::log_temp(300ms, "found nothing");
            }
        }
        // else...
    }

    // TODO: re-implement file-saving?
    imgui_str(rule_str);

    // TODO: better layout...
    ImGui::Separator();
    // TODO: +1 is clumsy
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
    ImGui::SameLine();
    iter_pair(
        "<|", "prev", "next", "|>", //
        [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
        [&] { recorder.set_last(); });
}

// TODO: "paste" should have a similar widget...
class file_nav_with_recorder {
    file_nav m_nav;
    std::optional<std::filesystem::path> m_file;
    rule_recorder m_recorder;

public:
    void display(rule_recorder& out) {
        // TODO: refresh... / reset(clear)...
        // TODO: set as current_path... ./ copy_path...
        // TODO: notify that all the operations are [read-only...]
        bool hit = false;

        if (!m_file) {
            ImGui::BeginDisabled();
        }
        // TODO: using childwindow only for padding...
        if (auto child = imgui_childwindow("Recorder", {},
                                           ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY)) {
            if (m_file) {
                // TODO: or wrapped full path?
                imgui_str(cpp17_u8string(m_file->filename()));
            } else {
                imgui_str("...");
            }

            // TODO: +1 is clumsy
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Total:%d At:%d", m_recorder.size(), m_recorder.pos() + 1);
            ImGui::SameLine();

            iter_pair(
                "<|", "prev", "next", "|>", //
                [&] { m_recorder.set_first(), hit = true; }, [&] { m_recorder.set_prev(), hit = true; },
                [&] { m_recorder.set_next(), hit = true; }, [&] { m_recorder.set_last(), hit = true; });
        }
        if (!m_file) {
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemClicked()) {
            assert(m_file);
            hit = true;
        }

        if (hit) {
            assert(m_file);
            out.take(m_recorder.current());
        }

        {
            // TODO: find better ways to show sync...
            const bool sync = m_file && (m_recorder.current() == out.current());
            const ImU32 col = sync ? IM_COL32_WHITE : ImGui::GetColorU32(ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
        }

        // TODO: better layout...
        if (auto sel = m_nav.display()) {
            auto result = legacy::extract_rules(load_binary(*sel, 1'000'000));
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                m_recorder.replace(std::move(result));
                out.take(m_recorder.current()); // Sync with `out`.
                m_file = std::move(sel);
            } else {
                logger::log_temp(500ms, "found nothing");
            }
        }
    }
};

// TODO: are there portable ways to convert argv to a valid filesystem::path (without messing up encodings)?
int main(int argc, char** argv) {
    app_backend::init();
    {
        char* base_path = SDL_GetBasePath();

        if (!base_path) {
            printf("Error: %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        const std::string path = base_path;
        assert(path.ends_with('\\') || path.ends_with('/'));
        SDL_free(base_path);

        const auto strdup = [](const std::string& str) {
            char* buf = new char[str.size() + 1];
            strcpy(buf, str.c_str());
            return buf;
        };

        file_nav::add_special_path(std::filesystem::u8path(path), "Exe path");

        // Avoid "imgui.ini" (and maybe also "imgui_log.txt") sprinkling everywhere.
        // TODO: IniFilename and LogFilename should be unconditionally fixed (even if not using base-path)
        // (wontfix) These memory leaks are negligible.
        ImGui::GetIO().IniFilename = strdup(path + "imgui.ini");
        ImGui::GetIO().LogFilename = strdup(path + "imgui_log.txt");

        // TODO: remove when finished...
        file_nav::add_special_path(R"(C:\*redacted*\Desktop\rulelists_new)", "Temp");
    }

    // TODO: rephrase...
    // Currently the program doesn't attempt to deal with navigation mode.
    // The controls and program-defined widgets are not taking nav-mode compatiblity into consideration.
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // X
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // X
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad));

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: works but blurry, and how to apply in project?
    // ImGui::GetIO().Fonts->AddFontFromFileTTF(R"(C:\*redacted*\Desktop\Deng.ttf)", 13, nullptr,
    //                                          ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

    // TODO: (temp) sync point is being {recorder.current(), locked}...
    rule_recorder recorder; // rule ~ current...
    legacy::lockT locked{};

    bool show_nav_window = true;
    file_nav_with_recorder nav;

    code_image icons;
    tile_image img;

    while (app_backend::begin_frame()) {
        // TODO: applying following logic; consider refining it.
        // (there should be a single sync point to represent current rule (and lock)...)
        // recorder is modified during display, but will synchronize with runner's before next frame.
        const legacy::ruleT rule = recorder.current();

        if (show_nav_window) {
            if (auto window = imgui_window("File nav", &show_nav_window)) {
                nav.display(recorder);
            }
        }

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        if (auto window = imgui_window("Tile", flags)) {
            ImGui::Checkbox("Nav window", &show_nav_window);
            ImGui::SameLine();
            // TODO: change color when is too fps is too low...
            ImGui::Text("   (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            show_target_rule(rule, recorder);
            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Rul")) {
                    if (auto out = stone_constraints()) {
                        auto& [rule, lock] = *out;
                        recorder.take(rule); // TODO: (temp) will be used at next frame...
                        locked = lock;
                    }
                    // TODO: let lock be dealt with like rule... (modifications are returned by value)
                    if (auto out = edit_rule(rule, locked, icons)) {
                        recorder.take(*out); // TODO: `edit_tile` uses the old rule at this frame... does this matter?
                    }

                    // TODO: This is used to pair with enter key and is somewhat broken...
                    // TODO: should enter set_next first?
                    if (imgui_keypressed(ImGuiKey_Apostrophe, false)) {
                        recorder.set_prev();
                    }
                }
                ImGui::TableNextColumn();
                // TODO: it seems this childwindow is not necessary?
                if (auto child = imgui_childwindow("Til")) {
                    ImGui::PushItemWidth(FixedItemWidth);
                    edit_tile(rule, locked, img);
                    ImGui::PopItemWidth();
                }
                ImGui::EndTable();
            }
        }

        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...
        logger::tempwindow();
        app_backend::end_frame();
    }

    app_backend::clear();
    return 0;
}
