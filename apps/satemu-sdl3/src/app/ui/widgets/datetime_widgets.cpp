#include "datetime_widgets.hpp"

#include <satemu/core/types.hpp>

#include <imgui.h>

#include <algorithm>

namespace app::ui::widgets {

bool DateTimeSelector(const char *id, util::datetime::DateTime &dateTime) {
    ImGui::PushID(id);

    static constexpr const char *kMonths[] = {"January", "February", "March",     "April",   "May",      "June",
                                              "July",    "August",   "September", "October", "November", "December"};

    const float framePadding = ImGui::GetStyle().FramePadding.x;
    const float frameHeight = ImGui::GetFrameHeight();

    const float digitWidth = [] {
        float width = 0.0f;
        char str[2] = {'0', '\0'};
        for (int i = 0; i < 9; i++) {
            str[0] = '0' + i;
            width = std::max(width, ImGui::CalcTextSize(str).x);
        }
        return width;
    }();

    const float monthWidth = [] {
        float width = 0.0f;
        for (const char *month : kMonths) {
            width = std::max(width, ImGui::CalcTextSize(month).x);
        }
        return width;
    }();

    bool changed = false;
    ImGui::SetNextItemWidth(digitWidth * 4 + framePadding * 2);
    if (ImGui::InputScalar("##datetime_year", ImGuiDataType_S32, &dateTime.year, nullptr, nullptr, "%02d")) {
        dateTime.year = std::clamp(dateTime.year, 1970, 2100);
        changed = true;
    }
    ImGui::SameLine(0, 2);
    ImGui::TextUnformatted("/");
    ImGui::SameLine(0, 2);
    int month = dateTime.month - 1;
    ImGui::SetNextItemWidth(monthWidth + framePadding * 2 + frameHeight);
    if (ImGui::Combo("##datetime_month", &month, kMonths, (int)std::size(kMonths), (int)std::size(kMonths))) {
        dateTime.month = std::clamp<uint8>(month + 1, 1, 12);
        changed = true;
    }
    ImGui::SameLine(0, 2);
    ImGui::TextUnformatted("/");
    ImGui::SameLine(0, 2);
    ImGui::SetNextItemWidth(digitWidth * 2 + framePadding * 2);
    if (ImGui::InputScalar("##datetime_day", ImGuiDataType_U8, &dateTime.day, nullptr, nullptr, "%02u")) {
        static constexpr const uint8 kMaxDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const bool leapDay = dateTime.month == 2               // February gets a leap year...
                             && dateTime.year % 4 == 0         // ... every 4 years...
                             && (dateTime.year % 100 != 0      // ... except on century years...
                                 || dateTime.year % 400 == 0); // ... that are not multiples of 400.
        dateTime.day = std::clamp<uint8>(dateTime.day, 1, kMaxDays[dateTime.month] + leapDay);
        changed = true;
    }
    ImGui::SameLine(0.0f, 15.0f);
    ImGui::SetNextItemWidth(digitWidth * 2 + framePadding * 2);
    if (ImGui::InputScalar("##datetime_hour", ImGuiDataType_U8, &dateTime.hour, nullptr, nullptr, "%02u")) {
        dateTime.hour = std::clamp<uint8>(dateTime.hour, 0, 23);
        changed = true;
    }
    ImGui::SameLine(0, 2);
    ImGui::TextUnformatted(":");
    ImGui::SameLine(0, 2);
    ImGui::SetNextItemWidth(digitWidth * 2 + framePadding * 2);
    if (ImGui::InputScalar("##datetime_minute", ImGuiDataType_U8, &dateTime.minute, nullptr, nullptr, "%02u")) {
        dateTime.minute = std::clamp<uint8>(dateTime.minute, 0, 59);
        changed = true;
    }
    ImGui::SameLine(0, 2);
    ImGui::TextUnformatted(":");
    ImGui::SameLine(0, 2);
    ImGui::SetNextItemWidth(digitWidth * 2 + framePadding * 2);
    if (ImGui::InputScalar("##datetime_second", ImGuiDataType_U8, &dateTime.second, nullptr, nullptr, "%02u")) {
        dateTime.second = std::clamp<uint8>(dateTime.second, 0, 59);
        changed = true;
    }

    ImGui::PopID();

    return changed;
}

} // namespace app::ui::widgets
