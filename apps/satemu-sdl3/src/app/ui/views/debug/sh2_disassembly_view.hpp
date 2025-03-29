#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <imgui.h>

namespace app::ui {

class SH2DisassemblyView {
public:
    SH2DisassemblyView(SharedContext &context, satemu::sh2::SH2 &sh2);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::SH2 &m_sh2;

    struct Colors {
#define C(r, g, b) (r / 255.0f), (g / 255.0f), (b / 255.0f), 1.0f
        struct Disassembly {
            ImVec4 address{C(217, 216, 237)}; // 06005432 .... ..
            ImVec4 bytes{C(237, 236, 216)};   // ........ 4132 ..
            ImVec4 ascii = bytes;             // ........ .... A2

            ImVec4 delaySlot{C(96, 112, 156)};        // _ (delay slot prefix)
            ImVec4 mnemonic{C(173, 216, 247)};        // mov nop rts ...
            ImVec4 illegalMnemonic{C(247, 191, 173)}; // (illegal)
            ImVec4 sizeSuffix{C(128, 145, 194)};      // b w l

            ImVec4 condPass{C(143, 240, 132)}; // t f (bt/bf) eq ne pz pl ... (cmp/<cond>) (pass)
            ImVec4 condFail{C(222, 140, 135)}; // t f (bt/bf) eq ne pz pl ... (cmp/<cond>) (fail)

            ImVec4 immediate{C(221, 247, 173)};    // #0x1234
            ImVec4 regRead{C(173, 247, 206)};      // r0..r15 gbr vbr ... @( ) (read)
            ImVec4 regWrite{C(215, 173, 247)};     // r0..r15 gbr vbr ... @( ) (write)
            ImVec4 regReadWrite{C(247, 206, 173)}; // r0..r15 gbr vbr ... @( ) (read+write)

            ImVec4 separator{C(186, 191, 194)}; // , (operators) . (size)
            ImVec4 addrInc{C(147, 194, 155)};   // + (@Rn+)
            ImVec4 addrDec{C(194, 159, 147)};   // - (@-Rn)

            ImVec4 pcIconColor{C(15, 189, 219)};
            ImVec4 prIconColor{C(17, 113, 237)};
            ImVec4 bkptIconColor{C(250, 52, 17)};
            ImVec4 wtptIconColor{C(193, 37, 250)};

            ImVec4 cursorBgColor{C(34, 61, 2)};
            ImVec4 pcBgColor{C(3, 61, 71)};
            ImVec4 prBgColor{C(6, 40, 84)};
            ImVec4 bkptBgColor{C(84, 15, 3)};
            ImVec4 wtptBgColor{C(62, 3, 84)};

        } disasm;

        struct Annotation {
            ImVec4 general{C(151, 154, 156)}; // -> ...
            ImVec4 condPass{C(93, 168, 89)};  // eq: Z ...
            ImVec4 condFail{C(184, 100, 95)}; // eq: Z ...
        } annotation;
#undef C
    } m_colors;

    struct Style {
        // Spacing between address/instruction bytes and mnemonics
        float disasmSpacing = 10.0f;
        // Spacing between mnemonics and annotations
        float disasmAnnotationSpacing = 32.0f;
        // Spacing between different annotation elements
        float annotationInnerSpacing = 20.0f;
    } m_style;

    struct Settings {
        bool displayOpcodeBytes = true;
        bool displayOpcodeAscii = false;
    } m_settings;
};

} // namespace app::ui
