// modal_builder.hpp - Fluent modal dialog builder with ok/cancel and danger mode
//
// Usage:
//   // Full builder (call every frame):
//   imgui_util::modal_builder("Delete Item")
//       .message("Are you sure you want to delete this item?")
//       .ok_button("Delete", [&]{ delete_item(); })
//       .cancel_button("Cancel", [&]{ cancel(); })
//       .danger()
//       .render(&show_modal);
//
//   // Convenience:
//   imgui_util::confirm_dialog("Confirm", "Really delete?", &open,
//       [&]{ do_delete(); }, [&]{ cancelled(); });
#pragma once

#include <functional>
#include <imgui.h>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/controls.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Fluent builder for modal popup dialogs.
    // Call render() exactly once per frame. render() handles OpenPopup / BeginPopupModal /
    // CloseCurrentPopup internally based on *open.
    class modal_builder {
    public:
        explicit modal_builder(const char *title) : title_(title) {}

        modal_builder &message(const char *text) {
            message_ = text;
            return *this;
        }

        modal_builder &body(std::function<void()> fn) {
            body_ = std::move(fn);
            return *this;
        }

        modal_builder &size(float w, float h) {
            width_  = w;
            height_ = h;
            return *this;
        }

        template<std::invocable F>
        modal_builder &ok_button(const char *label, F &&on_ok) {
            ok_label_ = label;
            on_ok_    = std::forward<F>(on_ok);
            return *this;
        }

        template<std::invocable F>
        modal_builder &cancel_button(const char *label, F &&on_cancel) {
            cancel_label_ = label;
            on_cancel_    = std::forward<F>(on_cancel);
            show_cancel_  = true;
            return *this;
        }

        modal_builder &danger(bool d = true) {
            danger_ = d;
            return *this;
        }

        void render(bool *open) {
            if (open != nullptr && *open) {
                ImGui::OpenPopup(title_);
            }

            if (width_ > 0.0f) {
                ImGui::SetNextWindowSize({width_, height_}, ImGuiCond_Appearing);
            }

            const ImGuiWindowFlags win_flags = (width_ > 0.0f) ? ImGuiWindowFlags_None
                                                                : ImGuiWindowFlags_AlwaysAutoResize;

            if (const popup_modal modal{title_, open, win_flags}) {
                if (message_ != nullptr) {
                    const text_wrap_pos wrap(0.0f);
                    ImGui::TextUnformatted(message_);
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                if (body_) {
                    body_();
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                // OK button
                {
                    const auto do_ok = [&] {
                        if (ImGui::Button(ok_label_, {120, 0})) {
                            if (on_ok_) on_ok_();
                            if (open != nullptr) *open = false;
                            ImGui::CloseCurrentPopup();
                        }
                    };
                    if (danger_) {
                        const style_color c1(ImGuiCol_Button, colors::error_dark);
                        const style_color c2(ImGuiCol_ButtonHovered, colors::error);
                        const style_color c3(ImGuiCol_ButtonActive, brighten(colors::error, 0.1f));
                        do_ok();
                    } else {
                        do_ok();
                    }
                }

                // Cancel button
                if (show_cancel_) {
                    ImGui::SameLine();
                    if (ImGui::Button(cancel_label_, {120, 0})) {
                        if (on_cancel_) on_cancel_();
                        if (open != nullptr) *open = false;
                        ImGui::CloseCurrentPopup();
                    }
                }

                // Keyboard shortcuts
                if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                    if (on_ok_) on_ok_();
                    if (open != nullptr) *open = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    if (on_cancel_) on_cancel_();
                    if (open != nullptr) *open = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

    private:
        const char            *title_        = "";
        const char            *message_      = nullptr;
        const char            *ok_label_     = "OK";
        const char            *cancel_label_ = "Cancel";
        bool                   danger_       = false;
        bool                   show_cancel_  = true;
        float                  width_        = 0.0f;
        float                  height_       = 0.0f;
        std::function<void()>  on_ok_;
        std::function<void()>  on_cancel_;
        std::function<void()>  body_;
    };

    // Convenience for simple yes/no confirmations.
    // Caller passes *open = true to show, callbacks fire on button press.
    template<std::invocable OnYes, std::invocable OnNo>
    void confirm_dialog(const char *title, const char *message, bool *open, OnYes &&on_yes, OnNo &&on_no) {
        modal_builder(title)
            .message(message)
            .ok_button("OK", std::forward<OnYes>(on_yes))
            .cancel_button("Cancel", std::forward<OnNo>(on_no))
            .render(open);
    }

} // namespace imgui_util
