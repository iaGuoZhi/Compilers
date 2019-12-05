#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/translate/tree.h"
#include "tiger/regalloc/regalloc.h"

namespace CG {

RA::Result Codegen(F::Frame* f, T::StmList* stmList);
}
#endif