#include "IRPrinter.hpp"

#include <cstddef>

#include "Type.hpp"

void dumpIRInstruction(std::ostream& os, const IRInstruction& instruction) {
    switch (instruction.op) {
        case IROp::ConstInt:
            os << "  " << instruction.dst
               << " = const_int " << instruction.intValue << '\n';
            return;

        case IROp::ConstBool:
            os << "  " << instruction.dst
               << " = const_bool "
               << (instruction.boolValue ? "true" : "false")
               << '\n';
            return;

        case IROp::Load:
            os << "  " << instruction.dst
               << " = load " << instruction.args.at(0) << '\n';
            return;

        case IROp::Store:
            os << "  store "
               << instruction.args.at(0) << ", "
               << instruction.args.at(1) << '\n';
            return;

        case IROp::Add:
        case IROp::Subtract:
        case IROp::Multiply:
        case IROp::Divide:
        case IROp::Less:
        case IROp::LessEqual:
        case IROp::Greater:
        case IROp::GreaterEqual:
        case IROp::Equal:
        case IROp::NotEqual:
            os << "  " << instruction.dst
               << " = " << toString(instruction.op)
               << " " << instruction.args.at(0)
               << ", " << instruction.args.at(1)
               << '\n';
            return;

        case IROp::Negate:
        case IROp::Not:
            os << "  " << instruction.dst
               << " = " << toString(instruction.op)
               << " " << instruction.args.at(0)
               << '\n';
            return;

        case IROp::Call:
            os << "  ";
            if (!instruction.dst.empty()) {
                os << instruction.dst << " = ";
            }
            os << "call " << instruction.callee << '(';

            for (std::size_t i = 0; i < instruction.args.size(); ++i) {
                if (i != 0) {
                    os << ", ";
                }
                os << instruction.args[i];
            }
            os << ")\n";
            return;

        case IROp::Jump:
            os << "  jump " << instruction.target << '\n';
            return;

        case IROp::Branch:
            os << "  branch "
               << instruction.args.at(0) << ", "
               << instruction.trueTarget << ", "
               << instruction.falseTarget << '\n';
            return;

        case IROp::Return:
            os << "  return";
            if (!instruction.args.empty()) {
                os << " " << instruction.args.at(0);
            }
            os << '\n';
            return;
    }
}

void dumpIR(std::ostream& os, const IRProgram& program) {
    for (const IRFunction& function : program.functions) {
        os << "fn " << function.name << '(';
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (i != 0) {
                os << ", ";
            }
            os << function.parameters[i].name
               << ": " << toString(function.parameters[i].type);
        }
        os << ") -> " << toString(function.returnType) << '\n';

        if (!function.locals.empty()) {
            os << "locals:\n";

            for (const IRLocal& local : function.locals) {
                os << "  " << local.name
                   << ": " << toString(local.type) << '\n';
            }
        }
        os << '\n';

        for (const IRBlock& block : function.blocks) {
            os << block.name << ":\n";

            for (const IRInstruction& instruction : block.instructions) {
                dumpIRInstruction(os, instruction);
            }
            os << '\n';
        }
    }
}