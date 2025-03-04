#include <satemu/hw/scu/scu_dsp.hpp>

#include <satemu/hw/scu/scu_defs.hpp>

namespace satemu::scu {

SCUDSP::SCUDSP(sys::Bus &bus)
    : m_bus(bus) {
    Reset(true);
}

void SCUDSP::Reset(bool hard) {
    if (hard) {
        programRAM.fill(0);
        for (auto &bank : dataRAM) {
            bank.fill(0);
        }
    }

    programExecuting = false;
    programPaused = false;
    programEnded = false;
    programStep = false;

    PC = 0;
    dataAddress = 0;

    nextPC = ~0;
    jmpCounter = 0;

    sign = false;
    zero = false;
    carry = false;
    overflow = false;

    CT.fill(0);
    incCT.fill(false);

    ALU.u64 = 0;
    AC.u64 = 0;
    P.u64 = 0;
    RX = 0;
    RY = 0;

    loopTop = 0;
    loopCount = 0;

    dmaRun = false;
    dmaToD0 = false;
    dmaHold = false;
    dmaCount = 0;
    dmaSrc = 0;
    dmaDst = 0;
    dmaReadAddr = 0;
    dmaWriteAddr = 0;
    dmaAddrInc = 0;
}

void SCUDSP::Run(uint64 cycles) {
    // TODO: proper cycle counting

    RunDMA(cycles);

    for (uint64 cy = 0; cy < cycles; cy++) {
        // Bail out if not executing
        if (!programExecuting && !programStep) {
            return;
        }

        // Bail out if paused
        if (programPaused) {
            return;
        }

        // Execute next command
        const uint32 command = programRAM[PC++];
        const uint32 cmdCategory = bit::extract<30, 31>(command);
        switch (cmdCategory) {
        case 0b00: Cmd_Operation(command); break;
        case 0b10: Cmd_LoadImm(command); break;
        case 0b11: Cmd_Special(command); break;
        }

        // Update PC
        if (jmpCounter > 0) {
            jmpCounter--;
            if (jmpCounter == 0) {
                PC = nextPC;
                nextPC = ~0;
            }
        }

        // Update CT0-3
        for (int i = 0; i < 4; i++) {
            if (incCT[i]) {
                CT[i]++;
                CT[i] &= 0x3F;
            }
        }
        incCT.fill(false);

        // Clear stepping flag to ensure the DSP only runs one command when stepping
        programStep = false;
    }
}

void SCUDSP::RunDMA(uint64 cycles) {
    // TODO: proper cycle counting

    // Bail out if DMA is not running
    if (!dmaRun) {
        return;
    }

    const bool toD0 = dmaToD0;
    uint32 addrD0 = toD0 ? dmaWriteAddr : dmaReadAddr;
    const Bus bus = GetBus(addrD0);
    if (bus == Bus::None) {
        dmaRun = false;
        return;
    }

    if (dmaToD0) {
        dspLog.trace("Running DSP DMA transfer: DSP -> {:08X} (+{:X}), {} longwords", addrD0, dmaAddrInc, dmaCount);
    } else {
        dspLog.trace("Running DSP DMA transfer: {:08X} -> DSP (+{:X}), {} longwords", addrD0, dmaAddrInc, dmaCount);
    }

    // Run transfer
    // TODO: should iterate through transfers based on cycle count
    for (uint32 i = 0; i < dmaCount; i++) {
        const uint32 ctIndex = toD0 ? dmaSrc : dmaDst;
        const bool useDataRAM = ctIndex <= 3; // else: use program RAM
        const uint32 ctAddr = useDataRAM ? CT[ctIndex] : 0;
        if (toD0) {
            // Data RAM -> D0
            const uint32 value = useDataRAM ? dataRAM[ctIndex][ctAddr] : programRAM[i & 0xFF];
            if (bus == Bus::ABus) {
                // A-Bus -> one 32-bit write
                m_bus.Write<uint32>(addrD0, value);
                addrD0 += dmaAddrInc;
            } else if (bus == Bus::BBus) {
                // B-Bus -> two 16-bit writes
                m_bus.Write<uint16>(addrD0 + 0, value >> 16u);
                m_bus.Write<uint16>(addrD0 + 2, value >> 0u);
                addrD0 += dmaAddrInc * 2;
            } else if (bus == Bus::WRAM) {
                // WRAM -> one 32-bit write
                m_bus.Write<uint32>(addrD0, value);
                addrD0 += dmaAddrInc;
            }
        } else {
            // D0 -> Data/Program RAM
            uint32 value = 0;
            if (bus == Bus::ABus) {
                // A-Bus -> one 32-bit read
                value = m_bus.Read<uint32>(addrD0);
                addrD0 += dmaAddrInc;
            } else if (bus == Bus::BBus) {
                // B-Bus -> two 16-bit reads
                value = m_bus.Read<uint16>(addrD0 + 0) << 16u;
                value |= m_bus.Read<uint16>(addrD0 + 2) << 0u;
                addrD0 += 4;
            } else if (bus == Bus::WRAM) {
                // WRAM -> one 32-bit read
                value = m_bus.Read<uint32>(addrD0);
                addrD0 += dmaAddrInc;
            }
            if (useDataRAM) {
                dataRAM[ctIndex][ctAddr] = value;
            } else {
                programRAM[i & 0xFF] = value;
            }
        }
        addrD0 &= 0x7FF'FFFC;
        if (useDataRAM) {
            CT[ctIndex]++;
            CT[ctIndex] &= 0x3F;
        }
    }

    // Update RA0/WA0 if not holding address
    if (!dmaHold) {
        if (toD0) {
            dmaWriteAddr = addrD0;
        } else {
            dmaReadAddr = addrD0;
        }
    }

    dmaRun = false;
}

FORCE_INLINE void SCUDSP::Cmd_Operation(uint32 command) {
    auto setZS32 = [&](uint32 value) {
        zero = value == 0;
        sign = static_cast<sint32>(value) < 0;
    };

    // ALU
    switch (bit::extract<26, 29>(command)) {
    case 0b0000: break;            // NOP
    case 0b0001: ALU_AND(); break; // AND
    case 0b0010: ALU_OR(); break;  // OR
    case 0b0011: ALU_XOR(); break; // XOR
    case 0b0100: ALU_ADD(); break; // ADD
    case 0b0101: ALU_SUB(); break; // SUB
    case 0b0110: ALU_AD2(); break; // AD2
    case 0b1000: ALU_SR(); break;  // SR
    case 0b1001: ALU_RR(); break;  // RR
    case 0b1010: ALU_SL(); break;  // SL
    case 0b1011: ALU_RL(); break;  // RL
    case 0b1111: ALU_RL8(); break; // RL8
    }

    // X-Bus
    //
    // X-Bus writes simultaneously to P and X in some cases:
    // bits
    // 25-23  executed operations
    //  000
    //  001
    //  010   MOV MUL,P
    //  011   MOV [s],P
    //  100               MOV [s],X
    //  101               MOV [s],X
    //  110   MOV MUL,P   MOV [s],X
    //  111   MOV [s],P   MOV [s],X
    if (bit::extract<23, 24>(command) == 0b10) {
        // MOV MUL,P
        P.s64 = static_cast<sint64>(RX) * static_cast<sint64>(RY);
    }
    if (bit::extract<23, 25>(command) >= 0b011) {
        const sint32 value = ReadSource(bit::extract<20, 22>(command));
        if (bit::extract<23, 24>(command) == 0b11) {
            // MOV [s],P
            P.s64 = static_cast<sint64>(value);
        }
        if (bit::extract<25>(command)) {
            // MOV [s],X
            RX = value;
        }
    }

    // Y-Bus
    //
    // Y-Bus writes simultaneously to A and Y in some cases:
    // bits
    // 19-17  executed operations
    // 000
    // 001    CLR A
    // 010    MOV ALU,A
    // 011    MOV [s],A
    // 100                MOV [s],Y
    // 101    CLR A       MOV [s],Y
    // 110    MOV ALU,A   MOV [s],Y
    // 111    MOV [s],A   MOV [s],Y
    if (bit::extract<17, 18>(command) == 0b01) {
        // CLR A
        AC.s64 = 0;
    } else if (bit::extract<17, 18>(command) == 0b10) {
        // MOV ALU,A
        AC.s64 = ALU.s64;
    }
    if (bit::extract<17, 19>(command) >= 0b11) {
        const sint32 value = ReadSource(bit::extract<14, 16>(command));
        if (bit::extract<17, 18>(command) == 0b11) {
            // MOV [s],A
            AC.s64 = static_cast<sint64>(value);
        }
        if (bit::extract<19>(command)) {
            // MOV [s],Y
            RY = value;
        }
    }

    // D1-Bus
    switch (bit::extract<12, 13>(command)) {
    case 0b01: // MOV SImm, [d]
    {
        const sint32 imm = bit::extract_signed<0, 7>(command);
        const uint8 dst = bit::extract<8, 11>(command);
        WriteD1Bus(dst, imm);
        break;
    }
    case 0b11: // MOV [s], [d]
    {
        const uint8 src = bit::extract<0, 3>(command);
        const uint8 dst = bit::extract<8, 11>(command);
        const uint32 value = ReadSource(src);
        WriteD1Bus(dst, value);
        break;
    }
    }
}

FORCE_INLINE void SCUDSP::Cmd_LoadImm(uint32 command) {
    const uint32 dst = bit::extract<26, 29>(command);
    sint32 imm;
    if (bit::extract<25>(command)) {
        // Conditional transfer
        imm = bit::extract_signed<0, 18>(command);

        const uint8 cond = bit::extract<19, 24>(command);
        if (!CondCheck(cond)) {
            return;
        }
    } else {
        // Unconditional transfer
        imm = bit::extract_signed<0, 24>(command);
    }

    WriteImm(dst, imm);
}

FORCE_INLINE void SCUDSP::Cmd_Special(uint32 command) {
    const uint32 cmdSubcategory = bit::extract<28, 29>(command);
    switch (cmdSubcategory) {
    case 0b00: Cmd_Special_DMA(command); break;
    case 0b01: Cmd_Special_Jump(command); break;
    case 0b10: Cmd_Special_LoopBottom(command); break;
    case 0b11: Cmd_Special_End(command); break;
    }
}

FORCE_INLINE void SCUDSP::Cmd_Special_DMA(uint32 command) {
    // Finish previous DMA transfer
    if (dmaRun) {
        RunDMA(1); // TODO: cycles?
    }

    dmaRun = true;
    dmaToD0 = bit::extract<12>(command);
    dmaHold = bit::extract<14>(command);

    // Get DMA transfer length
    if (bit::extract<13>(command)) {
        const uint8 ctIndex = bit::extract<0, 1>(command);
        const bool inc = bit::extract<2>(command);
        const uint32 ctAddr = CT[ctIndex];
        dmaCount = dataRAM[ctIndex][ctAddr];
        if (inc) {
            CT[ctIndex]++;
            CT[ctIndex] &= 0x3F;
        }
    } else {
        dmaCount = bit::extract<0, 7>(command);
    }

    // Get [RAM] source/destination register (CT) index and address increment
    const uint8 addrInc = bit::extract<15, 17>(command);
    if (dmaToD0) {
        // DMA [RAM],D0,SImm
        // DMA [RAM],D0,[s]
        // DMAH [RAM],D0,SImm
        // DMAH [RAM],D0,[s]
        dmaSrc = bit::extract<8, 9>(command);
        dmaAddrInc = (1 << addrInc) & ~1;
    } else if (bit::extract<10, 14>(command) == 0b00000) {
        // DMA D0,[RAM],SImm
        // Cannot write to program RAM, but can increment by up to 64 units
        dmaDst = bit::extract<8, 9>(command);
        dmaAddrInc = (1 << (addrInc & 0x2)) & ~1;
    } else {
        // DMA D0,[RAM],[s]
        // DMAH D0,[RAM],SImm
        // DMAH D0,[RAM],[s]
        dmaDst = bit::extract<8, 10>(command);
        dmaAddrInc = (1 << (addrInc & 0x2)) & ~1;
    }

    dspLog.trace("DSP DMA command: {:04X} @ {:02X}", command, PC);
}

FORCE_INLINE void SCUDSP::Cmd_Special_Jump(uint32 command) {
    const uint32 cond = bit::extract<19, 24>(command);
    if (cond != 0 && !CondCheck(cond)) {
        return;
    }

    const uint8 target = bit::extract<0, 7>(command);
    DelayedJump(target);
}

FORCE_INLINE void SCUDSP::Cmd_Special_LoopBottom(uint32 command) {
    if (loopCount != 0) {
        if (bit::extract<27>(command)) {
            // LPS
            DelayedJump(PC - 1);
        } else {
            // BTM
            DelayedJump(loopTop);
        }
    }
    loopCount--;
    loopCount &= 0xFFF;
}

FORCE_INLINE void SCUDSP::Cmd_Special_End(uint32 command) {
    const bool setEndIntr = bit::extract<27>(command);
    programExecuting = false;
    programEnded = true;
    if (setEndIntr) {
        m_cbTriggerDSPEnd();
    }
}

} // namespace satemu::scu
