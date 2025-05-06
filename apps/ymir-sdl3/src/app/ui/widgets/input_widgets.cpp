#include "input_widgets.hpp"

#include <app/events/gui_event_factory.hpp>

namespace app::ui::widgets {

InputCaptureWidget::InputCaptureWidget(SharedContext &context, UnboundActionsWidget &unboundActionsWidget)
    : m_context(context)
    , m_unboundActionsWidget(unboundActionsWidget) {}

void InputCaptureWidget::DrawInputBindButton(input::InputBind &bind, size_t elementIndex, void *context) {
    const std::string bindStr = input::ToHumanString(bind.elements[elementIndex]);
    const std::string label = fmt::format("{}##bind_{}_{}", bindStr, elementIndex, bind.action.id);
    const float availWidth = ImGui::GetContentRegionAvail().x;

    // Left-click engages bind mode
    if (ImGui::Button(label.c_str(), ImVec2(availWidth, 0))) {
        ImGui::OpenPopup("input_capture");
        m_capturing = true;
        switch (bind.action.kind) {
        case input::Action::Kind::Button: CaptureButton(bind, elementIndex, context); break;
        case input::Action::Kind::Axis1D: CaptureAxis1D(bind, elementIndex, context); break;
        case input::Action::Kind::Axis2D: CaptureAxis2D(bind, elementIndex, context); break;
        }
    }

    // Right-click erases a bind
    if (MakeDirty(ImGui::IsItemClicked(ImGuiMouseButton_Right))) {
        m_context.inputContext.CancelCapture();
        m_capturing = false;
        bind.elements[elementIndex] = {};
        m_context.EnqueueEvent(events::gui::RebindInputs());
    }
}

void InputCaptureWidget::DrawCapturePopup() {
    if (ImGui::BeginPopup("input_capture")) {
        if (m_closePopup) {
            m_closePopup = false;
            ImGui::CloseCurrentPopup();
        }
        switch (m_kind) {
        case input::Action::Kind::Button:
            ImGui::TextUnformatted("Press any key, mouse button or gamepad button to map it.\n\n"
                                   "Press Escape or click outside of this popup to cancel.");
            break;
        case input::Action::Kind::Axis1D:
            ImGui::TextUnformatted("Move any one-dimensional axis such as analog triggers to map it.\n\n"
                                   "Press Escape or click outside of this popup to cancel.");
            break;
        case input::Action::Kind::Axis2D:
            ImGui::TextUnformatted("Move any two-dimensional axis such as sticks to map it.\n\n"
                                   "Press Escape or click outside of this popup to cancel.");
            break;
        }
        ImGui::EndPopup();
    } else if (m_capturing) {
        m_context.inputContext.CancelCapture();
        m_capturing = false;
    }
}

void InputCaptureWidget::CaptureButton(input::InputBind &bind, size_t elementIndex, void *context) {
    m_kind = input::Action::Kind::Button;
    m_context.inputContext.Capture([=, this, &bind](const input::InputEvent &event) -> bool {
        if (!event.element.IsButton()) {
            return false;
        }
        if (event.element.type == input::InputElement::Type::KeyCombo) {
            if (event.element.keyCombo.key == input::KeyboardKey::None) {
                // Map key modifier combos without a key press when releasing the keys
                if (event.buttonPressed) {
                    return false;
                }
            } else {
                // Map other key combos when pressed
                if (!event.buttonPressed) {
                    return false;
                }
            }
        }

        if (bind.elements[elementIndex] == event.element) {
            // User bound the same input element as before; do nothing
            m_closePopup = true;
            return true;
        }

        bind.elements[elementIndex] = event.element;
        MakeDirty();
        m_unboundActionsWidget.Capture(m_context.settings.UnbindInput(event.element));
        m_context.EnqueueEvent(events::gui::RebindInputs());
        m_closePopup = true;
        return true;
    });
}

void InputCaptureWidget::CaptureAxis1D(input::InputBind &bind, size_t elementIndex, void *context) {
    m_kind = input::Action::Kind::Axis1D;
    m_context.inputContext.Capture([=, this, &bind](const input::InputEvent &event) -> bool {
        if (!event.element.IsAxis1D()) {
            return false;
        }
        if (abs(event.axis1DValue) < 0.5f) {
            return false;
        }

        if (bind.elements[elementIndex] == event.element) {
            // User bound the same input element as before; do nothing
            m_closePopup = true;
            return true;
        }

        bind.elements[elementIndex] = event.element;
        MakeDirty();
        m_unboundActionsWidget.Capture(m_context.settings.UnbindInput(event.element));
        m_context.EnqueueEvent(events::gui::RebindInputs());
        m_closePopup = true;
        return true;
    });
}

void InputCaptureWidget::CaptureAxis2D(input::InputBind &bind, size_t elementIndex, void *context) {
    m_kind = input::Action::Kind::Axis2D;
    m_context.inputContext.Capture([=, this, &bind](const input::InputEvent &event) -> bool {
        if (!event.element.IsAxis2D()) {
            return false;
        }
        const float d = event.axis2D.x * event.axis2D.x + event.axis2D.y * event.axis2D.y;
        if (d < 0.5f * 0.5f) {
            return false;
        }

        if (bind.elements[elementIndex] == event.element) {
            // User bound the same input element as before; do nothing
            m_closePopup = true;
            return true;
        }

        bind.elements[elementIndex] = event.element;
        MakeDirty();
        m_unboundActionsWidget.Capture(m_context.settings.UnbindInput(event.element));
        m_context.EnqueueEvent(events::gui::RebindInputs());
        m_closePopup = true;
        return true;
    });
}

void InputCaptureWidget::MakeDirty() {
    m_context.settings.MakeDirty();
}

bool InputCaptureWidget::MakeDirty(bool value) {
    if (value) {
        MakeDirty();
    }
    return value;
}

} // namespace app::ui::widgets
