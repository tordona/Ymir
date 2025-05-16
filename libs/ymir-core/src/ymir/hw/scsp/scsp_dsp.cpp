#include <ymir/hw/scsp/scsp_dsp.hpp>

#include <ymir/util/bit_ops.hpp>
#include <ymir/util/data_ops.hpp>
#include <ymir/util/inline.hpp>

#include <algorithm>
#include <bit>

namespace ymir::scsp {

FORCE_INLINE static uint32 FloatToInt(const uint16 value) {
    const uint32 signXor = static_cast<sint32>((value & 0x8000) << 16) >> 1;
    const uint32 exp = (value >> 11) & 0xF;

    uint32 ret = value & 0x7FF;
    if (exp < 12) {
        ret |= 0x800;
    }
    ret <<= 11 + 8;
    ret ^= signXor;
    ret = static_cast<sint32>(ret) >> (8 + std::min<uint32>(11, exp));

    return ret & 0xFFFFFF;
}

FORCE_INLINE static uint32 IntToFloat(const uint32 value) {
    const uint32 shiftedValue = value << 8;
    const uint32 signXor = static_cast<sint32>(shiftedValue) >> 31;
    uint32 ret;

    uint32 exp = std::min(0x1F, std::countl_zero(((shiftedValue ^ signXor) << 1) | (1 << 19)));
    uint32 shift = exp - (exp == 12);

    ret = static_cast<sint32>(shiftedValue) >> (19 - shift);
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

    // DSP program is aligned to operation 7, which processes slot i-6.
    // 4 DSP program steps per slot -> -6*4 = -24 = 104 (or 0x68) in modulo 128
    PC = 0x68;

    INPUTS = 0;

    SFT_REG = 0;
    FRC_REG = 0;
    Y_REG = 0;
    ADRS_REG = 0;

    MDEC_CT = 0;

    m_mixStackGen = 0;
    m_mixStackNull = 0xFFFF;

    m_readPending = false;
    m_readNOFL = false;
    m_readValue = 0;

    m_writePending = false;
    m_writeValue = 0;

    m_readWriteAddr = 0;
}

void DSP::Step() {
    DSPInstr instr = program[PC];

    if (instr.IRA <= 0x1F) {
        // MEMS area: 24 -> 24 bits
        INPUTS = soundMem[instr.IRA];
    } else if (instr.IRA <= 0x2F) {
        // MIXS area: 20 -> 24 bits
        INPUTS = mixStack[GetMIXSIndex(instr.IRA & 0xF) ^ 0x10] << 4;
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
            sgaOutput = -(sint32)sgaOutput;
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
        uint16 tmp = ReadWRAM();
        m_readValue = (m_readPending && m_readNOFL) ? (tmp << 8) : FloatToInt(tmp);
        m_readPending = false;
        m_readNOFL = false;
    } else if (m_writePending) {
        WriteWRAM();
        m_writePending = false;
    }

    uint16 addr = addrs[instr.MASA] + instr.NXADR;

    if (instr.ADREB) {
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
            ADRS_REG = shifterOut >> 12;
        } else {
            ADRS_REG = (inputs >> 16) & 0xFFF;
        }
    }

    ++PC;
    if (PC == 0x80) {
        PC = 0;
        if (m_writePending) {
            WriteWRAM();
            m_writePending = false;
        }

        // Swap MIXS buffers
        m_mixStackGen ^= 0x10;
        m_mixStackNull = 0xFFFF;

        MDEC_CT--;
    }
}

void DSP::DumpRegs(std::ostream &out) const {
    auto write = [&](const auto &reg) { out.write((const char *)&reg, sizeof(reg)); };
    write(ringBufferLeadAddress);
    write(ringBufferLength);
    write(PC);
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

void DSP::SaveState(state::SCSPDSP &state) const {
    for (size_t i = 0; i < program.size(); i++) {
        state.MPRO[i] = program[i].u64;
    }
    state.TEMP = tempMem;
    state.MEMS = soundMem;
    state.COEF = coeffs;
    state.MADRS = addrs;
    state.MIXS = mixStack;
    state.EFREG = effectOut;
    state.EXTS = audioInOut;

    state.MIXSGen = m_mixStackGen;
    state.MIXSNull = m_mixStackNull;

    state.RBP = ringBufferLeadAddress;
    state.RBL = ringBufferLength;

    state.PC = PC;
    state.INPUTS = INPUTS;

    state.SFT_REG = SFT_REG;
    state.FRC_REG = FRC_REG;
    state.Y_REG = Y_REG;
    state.ADRS_REG = ADRS_REG;

    state.MDEC_CT = MDEC_CT;

    state.readPending = m_readPending;
    state.readNOFL = m_readNOFL;
    state.readValue = m_readValue;

    state.writePending = m_writePending;
    state.writeValue = m_writeValue;

    state.readWriteAddr = m_readWriteAddr;
}

bool DSP::ValidateState(const state::SCSPDSP &state) const {
    return true;
}

void DSP::LoadState(const state::SCSPDSP &state) {
    for (size_t i = 0; i < program.size(); i++) {
        program[i].u64 = state.MPRO[i];
    }

    tempMem = state.TEMP;
    soundMem = state.MEMS;
    coeffs = state.COEF;
    addrs = state.MADRS;
    mixStack = state.MIXS;
    effectOut = state.EFREG;
    audioInOut = state.EXTS;

    m_mixStackGen = state.MIXSGen & 0x10;
    m_mixStackNull = state.MIXSNull;

    ringBufferLeadAddress = state.RBP;
    ringBufferLength = state.RBL;

    PC = state.PC;
    INPUTS = state.INPUTS;

    SFT_REG = state.SFT_REG;
    FRC_REG = state.FRC_REG;
    Y_REG = state.Y_REG;
    ADRS_REG = state.ADRS_REG;

    MDEC_CT = state.MDEC_CT;

    m_readPending = state.readPending;
    m_readNOFL = state.readNOFL;
    m_readValue = state.readValue;

    m_writePending = state.writePending;
    m_writeValue = state.writeValue;

    m_readWriteAddr = state.readWriteAddr;
}

uint16 DSP::ReadWRAM() const {
    const uint32 address = m_readWriteAddr * sizeof(uint16);
    if (address < 0x80000) {
        return util::ReadBE<uint16>(&m_WRAM[address]);
    } else {
        return 0;
    }
}

void DSP::WriteWRAM() {
    const uint32 address = m_readWriteAddr * sizeof(uint16);
    if (address < 0x80000) {
        util::WriteBE<uint16>(&m_WRAM[address], m_writeValue);
    }
}

} // namespace ymir::scsp
