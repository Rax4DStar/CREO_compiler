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
void AssemblyGenerator::emitProgram(
    const IRProgram& program,
    std::ostream& output
) {
    output << "global _start\n\n";
    output << "section .text\n\n";

    output << "_start:\n";
    output << "    call si_main\n";
    output << "    mov edi, eax\n";
    output << "    mov eax, 60\n";
    output << "    syscall\n\n";

    for (const IRFunction& function : program.functions) {
        emitFunction(function, output);
    }
}
std::string AssemblyGenerator::functionLabel(
    const std::string& name
) {
    return "creo_" + name;
}
std::string AssemblyGenerator::blockLabel(
    const std::string& functionName,
    const std::string& blockName
) {
    return ".L_" + functionName + "_" + blockName;
}
void AssemblyGenerator::allocateStackSlots(
    const IRFunction& function
) {
    _context = FunctionContext{};
    _context.returnLabel =
        ".L_" + function.name + "_return";

    for (const IRLocal& local : function.locals) {
        allocateSlot(local.name);
    }

    for (const IRBlock& block : function.blocks) {
        for (const IRInstruction& instruction : block.instructions) {
            if (!instruction.dst.empty()) {
                allocateSlot(instruction.dst);
            }
        }
    }
}
void AssemblyGenerator::allocateSlot(
    const std::string& name
) {
    if (_context.stackSlots.contains(name)) {
        return;
    }

    _context.nextOffset += 4;
    _context.stackSlots.emplace(
        name,
        _context.nextOffset
    );
}
int AssemblyGenerator::slotOffset(
    const std::string& name
) const {
    const auto found = _context.stackSlots.find(name);

    if (found == _context.stackSlots.end()) {
        throw std::runtime_error(
            "Assembly error: no stack slot for IR value '" +
            name + "'"
        );
    }
    return found->second;
}
int AssemblyGenerator::stackSize() const noexcept {
    constexpr int alignment = 16;
    const int size = _context.nextOffset;

    return (
        (size + alignment - 1) / alignment
    ) * alignment;
}
std::string AssemblyGenerator::stackOperand(
    const std::string& name
) const {
    return
        "dword [rbp - " +
        std::to_string(slotOffset(name)) +
        "]";
}
