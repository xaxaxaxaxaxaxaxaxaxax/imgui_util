// key_binding.hpp - Key binding capture widget for hotkey configuration
//
// Usage:
//   static imgui_util::key_combo combo{ImGuiKey_S, ImGuiMod_Ctrl};
//   if (imgui_util::key_binding_editor("Save", &combo)) {
//       // combo changed
//   }
//
//   // Display the combo as text:
//   auto text = imgui_util::to_string(combo);
//   ImGui::TextUnformatted(text.c_str());
//
// Click the button to enter capture mode. Press any key+modifier combination
// to bind it. Press Escape to cancel. The widget stores capture state in
// ImGui's state storage.
#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    struct key_combo {
        ImGuiKey      key  = ImGuiKey_None;
        ImGuiKeyChord mods = ImGuiMod_None;
    };

    // Format a key_combo as human-readable text (e.g. "Ctrl+Shift+K")
    [[nodiscard]]
    inline fmt_buf<128> to_string(const key_combo &combo) noexcept {
        if (combo.key == ImGuiKey_None && combo.mods == ImGuiMod_None) {
            return fmt_buf<128>{"None"};
        }

        fmt_buf<128> buf;
        if ((combo.mods & ImGuiMod_Ctrl) != 0) buf.append("Ctrl+");
        if ((combo.mods & ImGuiMod_Shift) != 0) buf.append("Shift+");
        if ((combo.mods & ImGuiMod_Alt) != 0) buf.append("Alt+");
        if ((combo.mods & ImGuiMod_Super) != 0) buf.append("Super+");

        if (combo.key != ImGuiKey_None) {
            buf.append("{}", ImGui::GetKeyName(combo.key));
        }

        return buf;
    }

    namespace detail {

        // Check whether a key is a modifier (Ctrl/Shift/Alt/Super)
        [[nodiscard]]
        constexpr bool is_modifier_key(const ImGuiKey key) noexcept {
            return key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
                   key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
                   key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
                   key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper;
        }

        // Scan for the currently held modifier flags
        [[nodiscard]]
        inline ImGuiKeyChord current_modifiers() noexcept {
            const auto &io = ImGui::GetIO();
            ImGuiKeyChord mods = ImGuiMod_None;
            if (io.KeyCtrl) mods |= ImGuiMod_Ctrl;
            if (io.KeyShift) mods |= ImGuiMod_Shift;
            if (io.KeyAlt) mods |= ImGuiMod_Alt;
            if (io.KeySuper) mods |= ImGuiMod_Super;
            return mods;
        }

    } // namespace detail

    // Key binding editor widget. Returns true when the binding changes.
    [[nodiscard]]
    inline bool key_binding_editor(const char *label, key_combo *combo) noexcept {
        ImGuiWindow *win = ImGui::GetCurrentWindow();
        if (win->SkipItems) return false;

        const ImGuiID     widget_id = win->GetID(label);
        const auto       &style     = ImGui::GetStyle();

        // Use ImGui state storage to track capture mode (0 = display, 1 = capturing)
        int *const capturing = ImGui::GetStateStorage()->GetIntRef(widget_id, 0);

        const float height    = ImGui::GetFrameHeight();
        const float btn_width = ImGui::CalcItemWidth();

        bool changed = false;

        if (*capturing != 0) {
            // Capture mode: show blinking prompt with active-colored button
            const style_color btn_bg(ImGuiCol_Button, style.Colors[ImGuiCol_FrameBgActive]);

            if (ImGui::Button("Press a key...", {btn_width, height})) {
                // Click again cancels
                *capturing = 0;
            }

            // Check for Escape to cancel
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                *capturing = 0;
            } else {
                // Scan for a non-modifier key press
                for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                    const auto key = static_cast<ImGuiKey>(k);
                    if (detail::is_modifier_key(key)) continue;

                    if (ImGui::IsKeyPressed(key)) {
                        combo->key  = key;
                        combo->mods = detail::current_modifiers();
                        *capturing  = 0;
                        changed     = true;
                        break;
                    }
                }
            }
        } else {
            // Display mode: show current binding
            const auto text = to_string(*combo);
            if (ImGui::Button(text.c_str(), {btn_width, height})) {
                *capturing = 1;
            }
        }

        // Label
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(label);

        return changed;
    }

} // namespace imgui_util
