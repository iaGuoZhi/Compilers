#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include "tiger/absyn/absyn.h"
#include "tiger/frame/frame.h"
// translate为semant管理着局部变量和静态函数嵌套

/* Forward Declarations */
namespace A {
class Exp;
}  // namespace A

namespace TR {

class Exp;
class ExpAndTy;
class ExpList;
class Level;
class Access;
class AccessList;
class Cx;

Level* Outermost();

F::FragList* TranslateProgram(A::Exp*);

}  // namespace TR

#endif
