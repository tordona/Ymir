#pragma once

#include "utils.hpp"

#include <ymir/util/bit_ops.hpp>

#include <istream>
#include <memory>
#include <span>
#include <string>
#include <variant>

struct OpcodeFetchError {
    std::string message;
};

struct OpcodeFetchEnd {};

template <typename TOpcode>
struct IOpcodeFetcher {
    virtual ~IOpcodeFetcher() = default;

    using Result = std::variant<TOpcode, OpcodeFetchError, OpcodeFetchEnd>;
    virtual Result Fetch() = 0;
};

template <typename TOpcode>
struct CommandLineOpcodeFetcher : public IOpcodeFetcher<TOpcode> {
    using Result = IOpcodeFetcher<TOpcode>::Result;

    CommandLineOpcodeFetcher(std::span<const std::string> args)
        : m_args(args) {}

    Result Fetch() final {
        if (m_index >= m_args.size()) {
            return OpcodeFetchEnd{};
        }

        auto &opcodeStr = m_args[m_index++];
        auto maybeOpcode = ParseHex<TOpcode>(opcodeStr);
        if (!maybeOpcode) {
            return OpcodeFetchError{fmt::format("Invalid opcode: {}", opcodeStr)};
        }

        return TOpcode{*maybeOpcode};
    }

private:
    size_t m_index = 0;
    std::span<const std::string> m_args;
};

template <typename TOpcode>
struct StreamOpcodeFetcher : public IOpcodeFetcher<TOpcode> {
    using Result = IOpcodeFetcher<TOpcode>::Result;

    StreamOpcodeFetcher(std::unique_ptr<std::istream> &&input, size_t offset, size_t length)
        : m_input(std::move(input)) {

        m_input->seekg(0, std::ios::end);
        const size_t size = m_input->tellg();
        m_remaining = std::min(length, size - offset);
        m_input->seekg(offset, std::ios::beg);
    }

    Result Fetch() final {
        if (m_remaining == 0) {
            return OpcodeFetchEnd{};
        }
        TOpcode opcode{};
        m_input->read((char *)&opcode, sizeof(TOpcode));
        opcode = bit::big_endian_swap(opcode);
        if (m_input->good() && sizeof(TOpcode) <= m_remaining) {
            m_remaining -= sizeof(TOpcode);
            return TOpcode{opcode};
        } else {
            m_remaining = 0;
            return OpcodeFetchEnd{};
        }
    }

private:
    std::unique_ptr<std::istream> m_input;
    size_t m_remaining;
};
