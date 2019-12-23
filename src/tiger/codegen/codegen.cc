#include "tiger/codegen/codegen.h"
#include<cstdio>
#include <map>
#include<sstream>

#define MAX_INST_LEN 60
#define TEMP_STR_LENGTH 100
static int rspoffset=0;
static int wordSize=8;
namespace CG {

TEMP::Map *layermap=TEMP::Map::Empty();
int framesize=0;
int max_offset=0;
TEMP::Label *func_label;
//固定寄存器
AS::InstrList *iList=nullptr,*iLast=nullptr;

/*munch functions*/
static void munchStm(T::Stm *stm);
static TEMP::Temp *munchExp(T::Exp *exp);
static TEMP::TempList *munchArgs(int cnt,T::ExpList *args);


static void emit(AS::Instr *inst)
{
  if(iLast!=nullptr)
  {
    iLast->tail=new AS::InstrList(inst,nullptr);
    iLast=iLast->tail;
  }else{
    iList=new AS::InstrList(inst,nullptr);
    iLast=iList;
  }
}
/*
static void calleeSave(bool saveOrRestore)
{
  static TEMP::TempList *saveTemp=nullptr;
  TEMP::TempList *calleeSaveRegs=F::calleeSaveRegs();
  std::string assm="movq `s0, `d0";
  //save callee saved regs
  if(saveOrRestore==true)
  {
    while(calleeSaveRegs)
    {
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      emit(new AS::MoveInstr(assm,new TEMP::TempList(temp,nullptr),calleeSaveRegs));
      calleeSaveRegs=calleeSaveRegs->tail;
      saveTemp=new TEMP::TempList(temp,saveTemp);
    }
  }
  else{
    while(calleeSaveRegs)
    {
      emit(new AS::MoveInstr(assm,new TEMP::TempList(calleeSaveRegs->head,nullptr),new TEMP::TempList(saveTemp->head,nullptr)));
      calleeSaveRegs=calleeSaveRegs->tail;
      saveTemp=saveTemp->tail;
    }
  }
}*/

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
  //init
  iList=nullptr;
  iLast=nullptr;
 
  AS::InstrList *instrlist;
  T::StmList *stmlist=stmList;

  func_label=f->label;
  
  //save callee saved registers
  std::string assm="movq `s0, `d0";
  //calleeSave(true);
  TEMP::Temp *saveRBX=TEMP::Temp::NewTemp();
  TEMP::Temp *saveRBP=TEMP::Temp::NewTemp();
  TEMP::Temp *saveR12=TEMP::Temp::NewTemp();
  TEMP::Temp *saveR13=TEMP::Temp::NewTemp();
  TEMP::Temp *saveR14=TEMP::Temp::NewTemp();
  TEMP::Temp *saveR15=TEMP::Temp::NewTemp();
  emit(new AS::MoveInstr(assm,new TEMP::TempList(saveRBX,nullptr),new TEMP::TempList(F::RBX(),nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(saveRBP,nullptr),new TEMP::TempList(F::RBP(),nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(saveR12,nullptr),new TEMP::TempList(F::R12(),nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(saveR13,nullptr),new TEMP::TempList(F::R13(),nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(saveR14,nullptr),new TEMP::TempList(F::R14(),nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(saveR15,nullptr),new TEMP::TempList(F::R15(),nullptr)));
 
  for(stmlist;stmlist;stmlist=stmlist->tail)
  {
    munchStm(stmlist->head);
  }

  //restore callee saved registers
  //calleeSave(false);
  emit(new AS::MoveInstr(assm,new TEMP::TempList(F::RBX(),nullptr),new TEMP::TempList(saveRBX,nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(F::RBP(),nullptr),new TEMP::TempList(saveRBP,nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(F::R12(),nullptr),new TEMP::TempList(saveR12,nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(F::R13(),nullptr),new TEMP::TempList(saveR13,nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(F::R14(),nullptr),new TEMP::TempList(saveR14,nullptr)));
  emit(new AS::MoveInstr(assm,new TEMP::TempList(F::R15(),nullptr),new TEMP::TempList(saveR15,nullptr)));

  return iList;
}

static void munchStm(T::Stm *stm)
{
  char inst[MAX_INST_LEN];
  std::string assm;

  switch (stm->kind)
  {
  case T::Stm::MOVE:
  {
    
    T::MoveStm *moveStm=(T::MoveStm*)stm;
    T::Exp *dst=moveStm->dst;
    T::Exp *src=moveStm->src;
    
    //move src, dst
    if(dst->kind==T::Exp::TEMP)
    {
      TEMP::Temp *e2=munchExp(src);
      T::TempExp *dsttempexp=(T::TempExp*) dst;

      assm="movq `s0, `d0";

      emit(new AS::MoveInstr(assm,new TEMP::TempList(dsttempexp->temp,nullptr),new TEMP::TempList(e2,nullptr)));  
      return;
    }
    //move src,mem
    if(dst->kind==T::Exp::MEM)
    {
      //fprintf(stdout,"-----------movestm------------\n");
      TEMP::Temp *srctemp=munchExp(src);
      T::MemExp *memexp=(T::MemExp*) dst;
      TEMP::Temp *memtemp=munchExp(memexp->exp);
     
      assm="movq `s0, (`s1)";
      emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(srctemp,new TEMP::TempList(memtemp,nullptr)),new AS::Targets(nullptr)));
      return;
    } 
    assert(0);
    return ;
  }
  case T::Stm::JUMP:
  {
    //fprintf(stdout,"------------------jump stm------------\n");
    T::JumpStm *jumpstm=(T::JumpStm*) stm;
    TEMP::LabelList *jumplist=jumpstm->jumps;
    TEMP::Label *label=jumpstm->exp->name;
    sprintf(inst,"jmp %s",TEMP::LabelString(label).c_str());
    assm=std::string(inst);
    emit(new AS::OperInstr(assm,nullptr,nullptr,new AS::Targets(jumplist)));
    return ;
  }
  case T::Stm::CJUMP:
  {
    //fprintf(stdout,"-----------------cjump stm--------------\n");
    T::CjumpStm *cjumpstm=(T::CjumpStm *)stm;
    TEMP::Temp *left=munchExp(cjumpstm->left);
    TEMP::Temp *right=munchExp(cjumpstm->right);
    TEMP::Label *trues=cjumpstm->true_label;
    TEMP::Label *falses=cjumpstm->false_label;


    assm="cmp `s0, `s1";
    emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(right,new TEMP::TempList(left,nullptr)),nullptr));
    switch(cjumpstm->op)
    {
      case T::EQ_OP: assm="je"; break;
      case T::NE_OP: assm="jne"; break;
      case T::LT_OP: assm="jl"; break;
      case T::GT_OP: assm="jg"; break;
      case T::LE_OP: assm="jle"; break;
      case T::GE_OP: assm="jge"; break;
      case T::ULT_OP: assm="jb"; break;
      case T::UGT_OP: assm="ja"; break;
      case T::ULE_OP: assm="jbe"; break;
      case T::UGE_OP: assm="jae"; break;
    }
    assm+=" ";
    assm+=TEMP::LabelString(trues);
    emit(new AS::OperInstr(assm,nullptr,nullptr,new AS::Targets(new TEMP::LabelList(trues,nullptr))));
    return;
  }
  case T::Stm::LABEL:
  {
    //fprintf(stdout,"-----------------label stm--------------\n");
    T::LabelStm *labelstm=(T::LabelStm *)stm;
    assm=TEMP::LabelString(labelstm->label);
    emit(new AS::LabelInstr(assm,labelstm->label));
    return;
  }
  case T::Stm::EXP:
  {
    //fprintf(stdout,"-----------------exp stm--------------\n");
    T::ExpStm *expstm=(T::ExpStm *) stm;
    T::Exp *exp=expstm->exp;
    munchExp(exp);
    return;
  }
  default:
  assert(0);
    break;
  }
}

static void munchArgs(T::ExpList *explist)
{
  T::ExpList *arglist=explist;

  //move args to rdi,rsi... for external calls
  TEMP::TempList *regList=F::ArgRegs();
  AS::InstrList *pushList=nullptr;
  while(regList&&explist)
  {
    TEMP::Temp *temp=munchExp(explist->head);
    emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(regList->head,nullptr),new TEMP::TempList(temp,nullptr)));
    regList=regList->tail;
    explist=explist->tail;
  }

  /******************
   * 栈帧结构
   * %rcx
   * %rdx
   * %rsi
   * %rdi   =========>  保存当前帧的顶部
   * 返回地址
   * 下方新的帧空间
   * ****************/
  regList=F::ArgRegs();
  while(arglist)
  {
    pushList=new AS::InstrList(new AS::OperInstr("pushq `s0",nullptr,new TEMP::TempList(regList->head,nullptr),new AS::Targets(nullptr)),pushList);
    arglist=arglist->tail;
    regList=regList->tail;
    rspoffset+=wordSize;
  }
  // reverse push order
  while(pushList)
  {
    emit(pushList->head);
    pushList=pushList->tail;
  }
  assert(explist==nullptr);
}

static TEMP::Temp *munchExp(T::Exp *exp)
{
  std::string assm;
  char inst[MAX_INST_LEN];
  switch (exp->kind)
  {
  //movq mem,dst
  case T::Exp::MEM:
  {
    //fprintf(stdout,"-----------------mem exp--------------\n");
    T::MemExp *memexp=(T::MemExp*) exp;
    TEMP::Temp *temp=TEMP::Temp::NewTemp();
    TEMP::Temp *memtmp=munchExp(memexp->exp);

    assm="movq (`s0), `d0";
    emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),new TEMP::TempList(memtmp,nullptr),new AS::Targets(nullptr)));
    return temp;
  }
  case T::Exp::BINOP:
  {
    //fprintf(stdout,"-----------------binop exp--------------\n");
    T::BinopExp *binopexp=(T::BinopExp*) exp;
    TEMP::Temp *left=munchExp(binopexp->left);
    TEMP::Temp *right=munchExp(binopexp->right);
    TEMP::Temp *temp=TEMP::Temp::NewTemp();

    //movq left,dst
    //addq right dst
    if(binopexp->op==T::PLUS_OP)
    { 
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("addq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(right,new TEMP::TempList(temp,nullptr)),new AS::Targets(nullptr)));
      return temp;
    }
    //movq left,dst
    //subq right,dst
    if(binopexp->op==T::MINUS_OP)
    {
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("subq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(right,new TEMP::TempList(temp,nullptr)),new AS::Targets(nullptr)));
      return temp;
    }

    //movq left,dst
    //imul right,dst
    if(binopexp->op==T::MUL_OP)
    {
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("imul `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(right,new TEMP::TempList(temp,nullptr)),new AS::Targets(nullptr)));
      return temp;
    }

     //movq left,%RAX
     //cltd #use RAX's sign bit to fill rdx
     //idivl right 
    if(binopexp->op==T::DIV_OP)
    {
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::RAX(),nullptr),new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("cltd",new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr)), new TEMP::TempList(F::RAX(),nullptr),new AS::Targets(nullptr)));
      emit(new AS::OperInstr("idivq `s0",new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr)),
        new TEMP::TempList(right,new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr))),new AS::Targets(nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr),new TEMP::TempList(F::RAX(),nullptr)));
      return temp;
    }  
  }
  //CONST(i)
  case T::Exp::CONST:
  {
    //fprintf(stdout,"-----------------const exp--------------\n");
      T::ConstExp *constexp=(T::ConstExp*) exp;
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      sprintf(inst,"movq $%d, `d0",constexp->consti);
      assm=std::string(inst);
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr), nullptr,nullptr));
      return temp;
  }
  //TEMP(t)
  case T::Exp::TEMP:
  {
    //fprintf(stdout,"-----------------temp exp--------------\n");
    T::TempExp *tempexp=(T::TempExp*) exp;
    return tempexp->temp;
  }
  case T::Exp::NAME:
  {
    //fprintf(stdout,"-----------------name exp--------------\n");
    T::NameExp *nameexp=(T::NameExp*) exp;
    TEMP::Temp *temp=TEMP::Temp::NewTemp();
  
    sprintf(inst, "leaq %s(%%rip), `d0", TEMP::LabelString(nameexp->name).c_str());
    assm=std::string(inst);
    emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),nullptr,nullptr));
    return temp;
  }
  

  case T::Exp::CALL:
  {
    //fprintf(stdout,"-----------------call exp--------------\n");
    T::CallExp *callexp=(T::CallExp *)exp;
    TEMP::Temp *temp=TEMP::Temp::NewTemp();

    munchArgs(callexp->args);
    TEMP::Label *label=((T::NameExp *)callexp->fun)->name;
    sprintf(inst,"call %s",TEMP::LabelString(label).c_str());
    assm=std::string(inst);
    //将caller save regs 作为CALL指令的目标寄存器，以便保留在活跃分析中的def中，让regalloc能知道这些寄存器被使用
    //caller saved regs

    emit(new AS::OperInstr(assm,F::callerSaveRegs(),nullptr,new AS::Targets(nullptr)));
    while(rspoffset>0)
    {
      rspoffset-=wordSize;
      emit(new AS::OperInstr("popq `d0",new TEMP::TempList(TEMP::Temp::NewTemp(),nullptr),nullptr,new AS::Targets(nullptr)));
    }
    sprintf(inst,"leaq %s_framesize(%%rsp),`d0 ",func_label->Name().c_str());
    emit(new AS::OperInstr(std::string(inst),new TEMP::TempList(F::R15(),nullptr),nullptr,new AS::Targets(nullptr)));
    emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr),new TEMP::TempList(F::RAX(),nullptr)));
    return temp;
  }
  default:
    assert(0);
    return nullptr;
    break;
  }
  assert(0);
}


}  // namespace CG