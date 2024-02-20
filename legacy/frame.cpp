#include "common.hpp"

// Never empty.
class recorderT {
    std::vector<legacy::compressT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.push_back(legacy::compress({legacy::game_of_life()}));
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    void update(const legacy::moldT& mold) {
        const legacy::compressT cmpr = legacy::compress(mold);
        if (cmpr != m_record[m_pos]) {
            m_record.push_back(cmpr);
            set_last();
        }
    }

    legacy::moldT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return legacy::decompress(m_record[m_pos]);
    }

    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

    void clear() {
        m_record = {m_record[m_pos]};
        m_pos = 0;
    }

private:
    void set_pos(int pos) { m_pos = std::clamp(pos, 0, size() - 1); }
};

static void assign_val(legacy::moldT& mold, legacy::extrT::valT& val) {
    if (val.lock) {
        mold.lock = *val.lock;
        mold.rule = val.rule;
    } else {
        // Clear the lock if val.rule doesn't fit well.
        if (!legacy::for_each_code_all_of(
                [&](legacy::codeT code) { return !mold.lock[code] || mold.rule[code] == val.rule[code]; })) {
            mold.lock = {};
        }
        mold.rule = val.rule;
    }
}

void frame(const code_image& icons, tile_image& img) {
    static recorderT recorder;

    ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...
    messenger::display();

    legacy::moldT current = recorder.current();
    bool update = false;

    static bool show_load = true;
    if (show_load) {
        // TODO: this should be controlled by load_rule ...
        ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(FLT_MAX, FLT_MAX));
        if (auto window = imgui_window("Load rule", &show_load, ImGuiWindowFlags_NoCollapse)) {
            if (auto out = load_rule()) {
                assign_val(current, *out);
                update = true;
            }
        }
    }

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (auto window = imgui_window("Main", flags)) {
        ImGui::Text("(%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

        // TODO: as `current` may have been changed by static_constraints, `current` may have been out-of-sync with
        // recorder at this frame... Does this matter?
        {
            const std::string rule_str = legacy::to_MAP_str(current.rule);

            // TODO: temp...
            if (ImGui::Button("Copy&lock")) {
                ImGui::SetClipboardText(legacy::to_MAP_str(current).c_str());
            }
            ImGui::SameLine();
            // TODO: better gui logic for copy... find ways to show feedback
            if (ImGui::Button("Copy")) {
                ImGui::SetClipboardText(rule_str.c_str());
            }
            ImGui::SameLine();
            // TODO: (temp) added back; remove when pasting is supported by load_rule...
            if (ImGui::Button("Paste")) {
                if (const char* str = ImGui::GetClipboardText()) {
                    if (auto out = legacy::extract_MAP_str(std::string_view(str)).val) {
                        assign_val(current, *out);
                        update = true;
                    }
                }
            }

            if (!show_load) {
                ImGui::SameLine();
                if (ImGui::Button("Load")) {
                    show_load = true;
                }
            }

            imgui_str(rule_str);

            // TODO: +1 is clumsy
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
            ImGui::SameLine();
            iter_pair(
                "<|", "prev", "next", "|>", //
                [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
                [&] { recorder.set_last(); });
            ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                recorder.clear();
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (auto child = imgui_childwindow("Rul", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (auto out = edit_rule(current, icons)) {
                    current = *out;
                    update = true;
                }

                // TODO: This is used to pair with enter key and is somewhat broken...
                if (imgui_keypressed(ImGuiKey_Semicolon, true)) {
                    recorder.set_prev();
                }
                if (imgui_keypressed(ImGuiKey_Apostrophe, true)) {
                    recorder.set_next();
                }
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_childwindow("Til", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (auto out = edit_tile(current.rule, img)) {
                    current.lock = *out;
                    update = true;
                }
            }
            ImGui::EndTable();
        }
    }

    if (update) {
        recorder.update(current);
    }
}
