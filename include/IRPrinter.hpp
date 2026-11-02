#pragma once

#include <ostream>

#include "IR.hpp"

void dumpIRInstruction(std::ostream& os, const IRInstruction& instruction);
void dumpIR(std::ostream& os, const IRProgram& program);