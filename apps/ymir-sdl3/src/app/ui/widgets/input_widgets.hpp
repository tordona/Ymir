#pragma once

#include <app/shared_context.hpp>

#include <app/input/input_bind.hpp>

namespace app::ui::widgets {

class InputCaptureWidget {
public:
    InputCaptureWidget(SharedContext &context);

    void DrawInputBindButton(input::InputBind &bind, size_t elementIndex);

    void DrawCapturePopup();

private:
    SharedContext &m_context;

    input::Action::Kind m_kind;
    bool m_closePopup = false;

    void CaptureButton(input::InputBind &bind, size_t elementIndex);
    void CaptureAxis1D(input::InputBind &bind, size_t elementIndex);
    void CaptureAxis2D(input::InputBind &bind, size_t elementIndex);

    void MakeDirty();
    bool MakeDirty(bool value);
};

} // namespace app::ui::widgets
