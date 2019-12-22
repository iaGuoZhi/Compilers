#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"
#include "tiger/util/util.h"
namespace AS{
  class Proc;
  class InstrList;
}

namespace F{
  class AccessList;
  class Access;
}
namespace F {
const int wordSize=8;


//描述那些可以存放在栈中或寄存器的形式参数和局部变量
class Access {
 public:
  // INFRAM 指出一个相对帧指针偏移为X的存储位置，INREG指出将使用的寄存器
  enum Kind { INFRAME, INREG };

  Kind kind;

  Access(Kind kind) : kind(kind) {}

  // Hints: You may add interface like
  //        `virtual T::Exp* ToExp(T::Exp* framePtr) const = 0`
  //virtual T::Exp* ToExp(T::Exp* framePtr) const=0;
};

//F_formals接口函数抽取由k个“访问”组成的一张表


class AccessList {
 public:
  Access *head;
  AccessList *tail;
  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};


//表示有关形式参数和分配在栈帧中局部变量的信息
/*
  *F_frame是一个包含以下内容的数据结构：
  * 1.所有形式参数的位置
  * 2.实现“view shift”所需要的指令
  * 3.迄今为止已分配的栈帧大小
  * 4.该函数开始点的机器代码指令
  * */
class Frame {
  // Base class
  public:
  class AccessList *formal;
  TEMP::Label *label;
  int size;  // used in F::allocLocal
  Frame(AccessList *formal,TEMP::Label *label,int size);
  class AccessList *getFormals();
  TEMP::Label *getLabel();
};
/*
 * Fragments
 */

class Frag {
 public:
  enum Kind { STRING, PROC };

  Kind kind;

  Frag(Kind kind) : kind(kind) {}
};

class StringFrag : public Frag {
 public:
  TEMP::Label *label;
  std::string str;

  StringFrag(TEMP::Label *label, std::string str)
      : Frag(STRING), label(label), str(str) {}
};

class ProcFrag : public Frag {
 public:
  T::Stm *body;
  Frame *frame;

  ProcFrag(T::Stm *body, Frame *frame) : Frag(PROC), body(body), frame(frame) {}
};

class FragList {
 public:
  Frag *head;
  FragList *tail;

  FragList(Frag *head, FragList *tail) : head(head), tail(tail) {}
};


F::Frame *newFrame(TEMP::Label *name,U::BoolList *boollist);

//return content of Frame
class AccessList *getFormals(Frame *frame);
TEMP::Label getName(Frame *frame);
Access *allocLocal(Frame *frame,bool escape);
int spill(Frame *frame);
T::Exp *Exp(Access *access,T::Exp *fp);

TEMP::Temp *SP(void);
TEMP::Temp *FP(void);
TEMP::Temp *RV(void);
TEMP::Temp *Zero(void);

TEMP::Temp *RSP(void);
TEMP::Temp *RAX(void);
TEMP::Temp *RBP(void);
TEMP::Temp *RBX(void);
TEMP::Temp *RDI(void);
TEMP::Temp *RSI(void);
TEMP::Temp *RDX(void);
TEMP::Temp *RCX(void);
TEMP::Temp *R8(void);
TEMP::Temp *R9(void);
TEMP::Temp *R10(void);
TEMP::Temp *R11(void);
TEMP::Temp *R12(void);
TEMP::Temp *R13(void);
TEMP::Temp *R14(void);
TEMP::Temp *R15(void);
TEMP::TempList *ArgRegs(void);

//TEMP::TempList *specialRegs(void);

TEMP::Temp *Arg(int index);
void add_register_to_map(TEMP::Map *frameMap);
//TEMP::TempList *Calleesaves(void);
//TEMP::TempList *Callersaves(void);
//TEMP::TempList *registers(void);
//used in liveness analysis
TEMP::TempList *callerSaveRegs();
TEMP::TempList *calleeSaveRegs();
TEMP::TempList *hardReg();
int hardRegSize();
std::string hardRegsString(int index);

//static TEMP::Map *frameMap;
//void InitTempMap();

char *prolog(F::Frame *frame);
char *epilog(F::Frame *frame);

T::Exp *externalCall(std::string s,T::ExpList *args);
AS::Proc *procEntryExit3(F::Frame *frame,AS::InstrList *body);


}  // namespace F

#endif