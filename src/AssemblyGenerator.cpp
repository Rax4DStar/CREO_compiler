#include "AssemblyGenerator.hpp"

#include <stdexcept>
#include <utility>

void AssemblyGenerator::generate(
    const IRProgram& program,
    std::ostream& output
) {
    validateProgram(program);
    emitProgram(program, output);

    if (!output) {
        throw std::runtime_error(
            "Assembly error: failed while writing output"
        );
    }
}
void AssemblyGenerator::validateProgram(
    const IRProgram& program
) const {
    bool hasMain = false;

    for (const IRFunction& function : program.functions) {
        if (function.name == "main") {
            hasMain = true;
        }

        validateFunction(function);
    }
    if (!hasMain) {
        throw std::runtime_error(
            "Assembly error: IR program has no main function"
        );
    }
}
void AssemblyGenerator::validateFunction(
    const IRFunction& function
) const {
    if (!function.parameters.empty()) {
        throw std::runtime_error(
            "Assembly error: function parameters are not "
            "supported in Chapter 7 ('" +
            function.name + "')"
        );
    }
    for (const IRBlock& block : function.blocks) {
        for (const IRInstruction& instruction : block.instructions) {
            validateInstruction(instruction);
        }
    }
}
void AssemblyGenerator::validateInstruction(
    const IRInstruction& instruction
) {
    if (!isSupported(instruction.op)) {
        throw std::runtime_error(
            "Assembly error: instruction '" +
            std::string(toString(instruction.op)) +
            "' is not supported in Chapter 7"
        );
    }

    switch (instruction.op) {
        case IROp::ConstInt:
        case IROp::ConstBool:
            requireDestination(instruction);
            requireArgumentCount(instruction, 0);
            return;

        case IROp::Load:
            requireDestination(instruction);
            requireArgumentCount(instruction, 1);
            return;

        case IROp::Store:
            requireArgumentCount(instruction, 2);
            return;

        case IROp::Add:
        case IROp::Subtract:
        case IROp::Multiply:
        case IROp::Divide:
            requireDestination(instruction);
            requireArgumentCount(instruction, 2);
            return;

        case IROp::Negate:
        case IROp::Not:
            requireDestination(instruction);
            requireArgumentCount(instruction, 1);
            return;

        case IROp::Return:
            if (instruction.args.size() > 1) {
                throw std::runtime_error(
                    "Assembly error: return expects zero or one argument"
                );
            }
            return;

        default:
            break;
    }
    throw std::runtime_error(
        "Assembly internal error: unvalidated instruction"
    );
}
void AssemblyGenerator::requireDestination(
    const IRInstruction& instruction
) {
    if (instruction.dst.empty()) {
        throw std::runtime_error(
            "Assembly error: instruction '" +
            std::string(toString(instruction.op)) +
            "' requires a destination"
        );
    }
}

void AssemblyGenerator::requireArgumentCount(
    const IRInstruction& instruction,
    std::size_t expected
) {
    if (instruction.args.size() != expected) {
        throw std::runtime_error(
            "Assembly error: instruction '" +
            std::string(toString(instruction.op)) +
            "' expects " + std::to_string(expected) +
            " argument(s), got " +
            std::to_string(instruction.args.size())
        );
    }
}
bool AssemblyGenerator::isSupported(
    IROp operation
) noexcept {
    switch (operation) {
        case IROp::ConstInt:
        case IROp::ConstBool:
        case IROp::Load:
        case IROp::Store:
        case IROp::Add:
        case IROp::Subtract:
        case IROp::Multiply:
        case IROp::Divide:
        case IROp::Negate:
        case IROp::Not:
        case IROp::Return:
            return true;

        default:
            return false;
    }
}