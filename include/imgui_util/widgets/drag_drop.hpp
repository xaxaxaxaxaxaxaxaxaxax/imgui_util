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
#include <string_view>
#include <type_traits>

#include "imgui_util/core/raii.hpp"

namespace imgui_util::drag_drop {

    static constexpr std::size_t max_payload_bytes = 1024;

    template<typename T>
    concept payload = std::is_trivially_copyable_v<T> && sizeof(T) <= max_payload_bytes;

    // Set a typed payload during a drag source scope
    template<typename T>
        requires payload<T>
    void set_payload(const char *type, const T &value, const ImGuiCond cond = 0) noexcept {
        ImGui::SetDragDropPayload(type, &value, sizeof(T), cond);
    }

    // Accept a typed payload during a drag target scope. Returns nullopt if no matching payload.
    template<typename T>
        requires payload<T>
    [[nodiscard]] auto accept_payload(const char *type, const ImGuiDragDropFlags flags = 0) noexcept
        -> std::optional<T> {
        if (const auto *const pl = ImGui::AcceptDragDropPayload(type, flags)) {
            T value;
            std::memcpy(&value, pl->Data, sizeof(T));
            return value;
        }
        return std::nullopt;
    }

    // Peek at payload without accepting (for preview during hover)
    template<typename T>
        requires payload<T>
    [[nodiscard]] auto peek_payload(const char *type) noexcept -> std::optional<T> {
        if (const auto *const pl = ImGui::GetDragDropPayload()) {
            if (pl->IsDataType(type)) {
                T value;
                std::memcpy(&value, pl->Data, sizeof(T));
                return value;
            }
        }
        return std::nullopt;
    }

    // Convenience: full drag source with preview text and payload
    template<typename T>
        requires payload<T>
    void source(const char *type, const T &value, const std::string_view preview_text,
                const ImGuiDragDropFlags flags = 0) noexcept {
        if (const drag_drop_source src{flags}) {
            set_payload(type, value);
            ImGui::TextUnformatted(preview_text.data(), preview_text.data() + preview_text.size());
        }
    }

} // namespace imgui_util::drag_drop
