#include <satemu/hw/scsp/scsp_dsp.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>
#include <satemu/util/inline.hpp>

#include <bit>

namespace satemu::scsp {

FORCE_INLINE static uint32 FloatToInt(const uint16 inv) {
    const uint32 signXor = static_cast<sint32>((inv & 0x8000) << 16) >> 1;
    const uint32 exp = (inv >> 11) & 0xF;

    uint32 ret = inv & 0x7FF;
    if (exp < 12) {
        ret |= 0x800;
    }
    ret <<= 11 + 8;
    ret ^= signXor;
    ret = static_cast<sint32>(ret) >> (8 + std::min<uint32>(11, exp));

    return ret & 0xFFFFFF;
}

FORCE_INLINE static uint32 IntToFloat(const uint32 inv) {
    const uint32 invsl8 = inv << 8;
    const uint32 sign_xor = static_cast<sint32>(invsl8) >> 31;
    uint32 ret;

    uint32 exp = std::min(0x1F, std::countl_zero(((invsl8 ^ sign_xor) << 1) | (1 << 19)));
    uint32 shift = exp - (exp == 12);

    ret = static_cast<sint32>(invsl8) >> (19 - shift);
    ret &= 0x87FF;
    ret |= exp << 11;

    return ret;
}

DSP::DSP(uint8 *ram)
    : m_WRAM(ram) {
    Reset();
}

void DSP::Reset() {
    program.fill({.u64 = 0});
    tempMem.fill(0);
    soundMem.fill(0);
    coeffs.fill(0);
    addrs.fill(0);
    mixStack.fill(0);
    effectOut.fill(0);
    audioInOut.fill(0);

    ringBufferLeadAddress = 0;
    ringBufferLength = 0;

    INPUTS = 0;

    SFT_REG = 0;
    FRC_REG = 0;
    Y_REG = 0;
    ADRS_REG = 0;

    MDEC_CT = 0;

    m_readPending = false;
    m_readNOFL = false;
    m_readValue = 0;

    m_writePending = false;
    m_writeValue = 0;

    m_readWriteAddr = 0;
}

void DSP::Run() {
    for (DSPInstr instr : program) {
        if (instr.IRA <= 0x1F) {
            // MEMS area: 24 -> 24 bits
            INPUTS = soundMem[instr.IRA];
        } else if (instr.IRA <= 0x2F) {
            // MIXS area: 20 -> 24 bits
            INPUTS = mixStack[instr.IRA & 0xF] << 4;
        } else if (instr.IRA <= 0x31) {
            // EXTS area: 16 -> 24 bits
            INPUTS = audioInOut[instr.IRA & 0x1] << 8;
        }

        const uint8 tempReadAddr = (instr.TRA + MDEC_CT) & 0x7F;
        const uint8 tempWriteAddr = (instr.TWA + MDEC_CT) & 0x7F;

        const sint32 inputs = bit::sign_extend<24>(INPUTS);
        const sint32 temp = bit::sign_extend<24>(tempMem[tempReadAddr]);

        const sint32 xselInputs[2] = {temp, inputs};
        const uint16 yselInputs[4] = {
            FRC_REG,
            coeffs[instr.CRA],
            static_cast<uint16>(bit::extract<11, 23>(Y_REG)),
            static_cast<uint16>(bit::extract<4, 15>(Y_REG)),
        };

        if (instr.YRL) {
            Y_REG = bit::extract<0, 23>(inputs);
        }

        sint32 shifterOut = static_cast<uint32>(bit::sign_extend<26>(SFT_REG)) << (instr.SHFT0 ^ instr.SHFT1);
        if (instr.SHFT1 == 0) {
            shifterOut = std::clamp(shifterOut, -0x800000, 0x7FFFFF);
        }
        shifterOut &= 0xFFFFFF;

        if (instr.FRCL) {
            if (instr.SHFT0 & instr.SHFT1) {
                FRC_REG = bit::extract<0, 11>(shifterOut);
            } else {
                FRC_REG = bit::extract<11, 23>(shifterOut);
            }
        }

        const uint32 product = (bit::sign_extend<13, sint64>(yselInputs[instr.YSEL]) * xselInputs[instr.XSEL]) >> 12;

        uint32 sgaOutput;
        if (instr.ZERO) {
            sgaOutput = 0;
        } else {
            if (instr.BSEL) {
                sgaOutput = SFT_REG;
            } else {
                sgaOutput = temp;
            }
            if (instr.NEGB) {
                sgaOutput = -sgaOutput;
            }
        }
        SFT_REG = (product + sgaOutput) & 0x3FFFFFF;

        if (instr.EWT) {
            effectOut[instr.EWA] = shifterOut >> 8;
        }
        if (instr.TWT) {
            tempMem[tempWriteAddr] = shifterOut;
        }
        if (instr.IWT) {
            soundMem[instr.IWA] = m_readValue;
        }

        if (m_readPending) {
            uint16 tmp = ReadWRAM(m_readWriteAddr * sizeof(uint16));
            m_readValue = (m_readPending && m_readNOFL) ? (tmp << 8) : FloatToInt(tmp);
            m_readPending = false;
            m_readNOFL = false;
        } else if (m_writePending) {
            WriteWRAM(m_readWriteAddr * sizeof(uint16), m_writeValue);
            m_writePending = false;
        }

        uint16 addr = addrs[instr.MASA] + instr.NXADDR;

        if (instr.ADRGB) {
            addr += bit::sign_extend<12>(ADRS_REG);
        }

        if (!instr.TABLE) {
            addr = (addr + MDEC_CT) & ((0x2000 << ringBufferLength) - 1);
        }

        m_readWriteAddr = (addr + (ringBufferLeadAddress << 12)) & 0x7FFFF;

        if (instr.MRD) {
            m_readPending = true;
            m_readNOFL = instr.NOFL;
        }
        if (instr.MWT) {
            m_writePending = true;
            m_writeValue = instr.NOFL ? (shifterOut >> 8) : IntToFloat(shifterOut);
        }

        if (instr.ADRL) {
            if (instr.SHFT0 & instr.SHFT1) {
                ADRS_REG = (inputs >> 16) & 0xFFF;
            } else {
                ADRS_REG = shifterOut >> 12;
            }
        }
    }

    if (MDEC_CT == 0) {
        MDEC_CT = 0x2000 << ringBufferLength;
    }
    MDEC_CT--;

    mixStack.fill(0);
}

void DSP::DumpRegs(std::ostream &out) {
    auto write = [&](const auto &reg) { out.write((const char *)&reg, sizeof(reg)); };
    write(ringBufferLeadAddress);
    write(ringBufferLength);
    write(INPUTS);

    write(SFT_REG);
    write(FRC_REG);
    write(Y_REG);
    write(ADRS_REG);

    write(MDEC_CT);

    write(m_readPending);
    write(m_readNOFL);
    write(m_readValue);

    write(m_writePending);
    write(m_writeValue);

    write(m_readWriteAddr);
}

uint16 DSP::ReadWRAM(uint32 address) {
    return util::ReadBE<uint16>(&m_WRAM[address & 0x7FFFF]);
}

void DSP::WriteWRAM(uint32 address, uint16 value) {
    util::WriteBE<uint16>(&m_WRAM[address & 0x7FFFF], value);
}

} // namespace satemu::scsp
