/// @file inline_edit.hpp
/// @brief Click-to-edit text label widget.
///
/// Renders as TextUnformatted normally. Double-click enters edit mode (InputText).
/// Enter commits, Escape cancels. Returns true when editing commits.
/// All state is stored in ImGui state storage keyed by str_id.
///
/// Usage:
/// @code
///   static std::string name = "Untitled";
///   if (imgui_util::inline_edit("##name", name)) {
///       // user committed edit (pressed Enter)
///   }
///
///   // With explicit width:
///   if (imgui_util::inline_edit("##title", title, 200.0f)) { ... }
/// @endcode
#pragma once

#include <algorithm>
#include <array>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    /**
     * @brief Click-to-edit text label widget.
     *
     * Renders as plain text normally. Double-click enters edit mode (InputText).
     * Enter commits the edit, Escape cancels. State is stored in ImGui state
     * storage keyed by @p str_id.
     *
     * @param str_id  ImGui ID for state storage.
     * @param text    String to display and edit in-place.
     * @param width   Explicit widget width, or 0 for auto-sizing.
     * @return True when the user commits an edit (presses Enter).
     */
    [[nodiscard]] inline bool inline_edit(const char *str_id, std::string &text, const float width = 0.0f) noexcept {
        if (const auto *const win = ImGui::GetCurrentWindow(); win->SkipItems) return false;

        const id scope{str_id};

        auto *const   storage    = ImGui::GetStateStorage();
        const ImGuiID editing_id = ImGui::GetID("##editing");

        auto *const editing_val = storage->GetIntRef(editing_id, 0);
        const bool  is_editing  = *editing_val != 0;

        bool committed = false;

        if (is_editing) {
            // Single static buffer -- only one InputText can be focused at a time
            std::array<char, 256> edit_buf{};

            // Initialize buffer on first edit frame
            const ImGuiID init_id  = ImGui::GetID("##init");
            auto *const   init_val = storage->GetIntRef(init_id, 0);
            if (*init_val == 0) {
                const auto len = std::min(text.size(), edit_buf.size() - 1);
                std::ranges::copy_n(text.data(), static_cast<std::ptrdiff_t>(len), edit_buf.data());
                edit_buf[len] = '\0';
                *init_val     = 1;
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::SetNextItemWidth(width > 0.0f ? width : -1.0f);

            if (ImGui::InputText("##input", edit_buf.data(), edit_buf.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                text         = edit_buf.data();
                *editing_val = 0;
                *init_val    = 0;
                committed    = true;
            }

            // Escape cancels
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                *editing_val = 0;
                *init_val    = 0;
            }

            // Cancel when InputText loses focus (click away). IsItemDeactivated only
            // fires after the widget was active, avoiding the first-frame false trigger.
            if (!committed && *editing_val != 0 && ImGui::IsItemDeactivated()) {
                *editing_val = 0;
                *init_val    = 0;
            }
        } else {
            // Display mode: TextUnformatted with InvisibleButton for double-click detection
            const auto text_size = ImGui::CalcTextSize(text.c_str());
            const auto btn_w     = width > 0.0f ? width : text_size.x + 1.0f;
            const auto btn_h     = text_size.y;

            const auto pos = ImGui::GetCursorScreenPos();
            ImGui::TextUnformatted(text.c_str());

            // Overlay invisible button for double-click
            ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton("##dbl", ImVec2(btn_w, btn_h));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                *editing_val = 1;
            }
        }

        return committed;
    }

} // namespace imgui_util
