// drag_drop.hpp - Type-safe drag-drop payload helpers
//
// Usage:
//   // In a drag source:
//   imgui_util::drag_drop::source("ITEM", my_index, "Dragging item");
//
//   // In a drag target:
//   if (const imgui_util::drag_drop_target tgt{}) {
//       if (auto val = imgui_util::drag_drop::accept_payload<int>("ITEM")) {
//           handle_drop(*val);
//       }
//   }
//
//   // Peek without accepting (for hover preview):
//   if (auto val = imgui_util::drag_drop::peek_payload<int>("ITEM")) { ... }
//
// Uses existing RAII drag_drop_source/drag_drop_target wrappers from core/raii.hpp.
#pragma once

#include <cstring>
#include <imgui.h>
#include <optional>
#include <type_traits>

#include "imgui_util/core/raii.hpp"

namespace imgui_util::drag_drop {

    // Set a typed payload during a drag source scope
    template<typename T>
        requires std::is_trivially_copyable_v<T>
    void set_payload(const char *type, const T &value, ImGuiCond cond = 0) {
        ImGui::SetDragDropPayload(type, &value, sizeof(T), cond);
    }

    // Accept a typed payload during a drag target scope. Returns nullopt if no matching payload.
    template<typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]]
    std::optional<T> accept_payload(const char *type, ImGuiDragDropFlags flags = 0) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(type, flags)) {
            T value;
            std::memcpy(&value, payload->Data, sizeof(T));
            return value;
        }
        return std::nullopt;
    }

    // Peek at payload without accepting (for preview during hover)
    template<typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]]
    std::optional<T> peek_payload(const char *type) {
        if (const ImGuiPayload *payload = ImGui::GetDragDropPayload()) {
            if (payload->IsDataType(type)) {
                T value;
                std::memcpy(&value, payload->Data, sizeof(T));
                return value;
            }
        }
        return std::nullopt;
    }

    // Convenience: full drag source with preview text and payload
    template<typename T>
        requires std::is_trivially_copyable_v<T>
    void source(const char *type, const T &value, const char *preview_text,
                ImGuiDragDropFlags flags = 0) {
        if (const drag_drop_source src{flags}) {
            set_payload(type, value);
            ImGui::TextUnformatted(preview_text);
        }
    }

} // namespace imgui_util::drag_drop
