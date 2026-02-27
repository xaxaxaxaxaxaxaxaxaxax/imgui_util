// confirm_button.hpp - Click-to-arm, click-again-to-confirm button for destructive actions
//
// Usage:
//   if (imgui_util::confirm_button("Delete", "##del_item")) {
//       // user confirmed deletion
//   }
//
//   // With custom timeout and armed color:
//   if (imgui_util::confirm_button("Reset All", "##reset", 5.0f, {1.0f, 0.2f, 0.2f, 1.0f})) { ... }
//
// First click arms the button (turns red, shows "Click again to confirm").
// Second click within timeout returns true. Timeout expiration resets to normal.
// All state is stored in ImGui state storage keyed by str_id.
#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Returns true when the user confirms (second click within timeout).
    [[nodiscard]] inline bool confirm_button(const char *label, const char *str_id, const float timeout_sec = 3.0f,
                                             const ImVec4 &armed_color = colors::error) noexcept {
        if (const ImGuiWindow *const win = ImGui::GetCurrentWindow(); win->SkipItems) return false;

        const id scope{str_id};

        auto *const   storage  = ImGui::GetStateStorage();
        const ImGuiID armed_id = ImGui::GetID("##armed");
        const ImGuiID time_id  = ImGui::GetID("##arm_time");

        int *const   armed_val = storage->GetIntRef(armed_id, 0);
        float *const arm_time  = storage->GetFloatRef(time_id, 0.0f);

        const bool is_armed = *armed_val != 0;
        const auto now      = static_cast<float>(ImGui::GetTime());

        // Check timeout
        if (is_armed && now - *arm_time > timeout_sec) {
            *armed_val = 0;
        }

        bool confirmed = false;

        if (*armed_val != 0) {
            // Armed state: show confirmation button
            const fmt_buf<128> armed_label("Confirm {}##{}", label, str_id);
            const style_colors sc{
                {ImGuiCol_Button, armed_color},
                {ImGuiCol_ButtonHovered,
                 ImVec4{armed_color.x * 1.1f, armed_color.y * 1.1f, armed_color.z * 1.1f, armed_color.w}},
                {ImGuiCol_ButtonActive,
                 ImVec4{armed_color.x * 0.9f, armed_color.y * 0.9f, armed_color.z * 0.9f, armed_color.w}},
            };
            if (ImGui::Button(armed_label.c_str())) {
                *armed_val = 0;
                confirmed  = true;
            }
        } else {
            // Normal state
            const fmt_buf<128> btn_label("{}##{}", label, str_id);
            if (ImGui::Button(btn_label.c_str())) {
                *armed_val = 1;
                *arm_time  = now;
            }
        }

        return confirmed;
    }

} // namespace imgui_util
