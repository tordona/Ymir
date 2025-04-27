#pragma once

#include "utils.hpp"

#include <ymir/util/bit_ops.hpp>

#include <istream>
#include <memory>
#include <span>
#include <string>
#include <variant>

// ---------------------------------------------------------------------------------------------------------------------
// Return types

struct OpcodeFetchError {
    std::string message;
};

struct OpcodeFetchEnd {};

template <typename TOpcode>
using OpcodeFetchResult = std::variant<TOpcode, OpcodeFetchError, OpcodeFetchEnd>;

// ---------------------------------------------------------------------------------------------------------------------
// Opcode fetcher interface

template <typename TOpcode>
struct IOpcodeFetcher {
    virtual ~IOpcodeFetcher() = default;

    using Result = OpcodeFetchResult<TOpcode>;
    virtual Result Fetch() = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
// Command line opcode fetcher templates

template <typename TOpcode>
struct SimpleCommandLineOpcodeParser {
    static OpcodeFetchResult<TOpcode> Parse(std::string_view arg) {
        auto maybeOpcode = ParseHex<TOpcode>(arg);
        if (!maybeOpcode) {
            return OpcodeFetchError{fmt::format("Invalid opcode: {}", arg)};
        }
        return TOpcode{*maybeOpcode};
    }
};

template <typename TOpcode, typename TParser = SimpleCommandLineOpcodeParser<TOpcode>>
    requires requires {
        { TParser::Parse(std::declval<std::string_view>()) } -> std::same_as<OpcodeFetchResult<TOpcode>>;
    }
struct CommandLineOpcodeFetcher : public IOpcodeFetcher<TOpcode> {
    using Result = IOpcodeFetcher<TOpcode>::Result;

    CommandLineOpcodeFetcher(std::span<const std::string> args)
        : m_args(args) {}

    Result Fetch() final {
        if (m_index >= m_args.size()) {
            return OpcodeFetchEnd{};
        }

        auto &opcodeStr = m_args[m_index++];
        return TParser::Parse(opcodeStr);
    }

private:
    size_t m_index = 0;
    std::span<const std::string> m_args;
};

// ---------------------------------------------------------------------------------------------------------------------
// std::istream opcode fetcher templates

template <typename TOpcode, std::endian endianness = std::endian::big>
struct SimpleStreamOpcodeParser {
    static TOpcode Parse(std::istream &input) {
        TOpcode opcode{};
        input.read((char *)&opcode, sizeof(TOpcode));
        return bit::endian_swap<endianness>(opcode);
    }
};

template <typename TOpcode, typename TParser = SimpleStreamOpcodeParser<TOpcode>>
    requires requires {
        { TParser::Parse(std::declval<std::istream &>()) } -> std::same_as<TOpcode>;
    }
struct StreamOpcodeFetcher : public IOpcodeFetcher<TOpcode> {
    using Result = IOpcodeFetcher<TOpcode>::Result;

    StreamOpcodeFetcher(std::unique_ptr<std::istream> &&input, size_t offset, size_t length)
        : m_input(std::move(input)) {

        m_input->seekg(0, std::ios::end);
        const size_t size = m_input->tellg();
        m_input->seekg(offset, std::ios::beg);
        m_endPos = std::min(offset + length, size);
    }

    Result Fetch() final {
        auto pos = m_input->tellg();
        if (pos >= m_endPos) {
            return OpcodeFetchEnd{};
        }
        if (!m_input) {
            std::error_code error{errno, std::generic_category()};
            return OpcodeFetchError{error.message()};
        }

        TOpcode opcode = TParser::Parse(*m_input);
        pos = m_input->tellg();
        if (m_input->good() && pos < m_endPos) {
            return opcode;
        } else {
            return OpcodeFetchEnd{};
        }
    }

private:
    std::unique_ptr<std::istream> m_input;
    size_t m_endPos;
};
