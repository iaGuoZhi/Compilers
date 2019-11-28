#include "tiger/codegen/codegen.h"


#define MAX_INST_LEN 40
#define TEMP_STR_LENGTH 100

static int wordSize=8;
namespace CG {


static AS::InstrList *iList=nullptr,*iLast=nullptr;

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

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
  // TODO: Put your codes here (lab6).
  AS::InstrList *list=nullptr;
  T::StmList *sl;
  return nullptr;
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
    if(dst->kind==T::Exp::MEM)
    {
      T::MemExp *memDstExp=(T::MemExp *)dst;
      if(memDstExp->exp->kind==T::Exp::BINOP)
      {
        T::BinopExp *binopExp=(T::BinopExp *)memDstExp->exp;
        //MOVE(MEM(BINOP(PLUS,e1,CONST(i))),e2)
        if(binopExp->op==T::PLUS_OP&&binopExp->right->kind==T::Exp::CONST)
        {
          TEMP::Temp *temp=TEMP::Temp::NewTemp();
          TEMP::Temp *lefttemp=munchExp(binopExp->left);
          T::ConstExp *rightconstexp=(T::ConstExp *)binopExp->right;
          TEMP::Temp *srctemp=munchExp(src);
         
          sprintf(inst,"movq `s0, %d(`s1)",rightconstexp->consti);
          assm=std::string(inst);
          emit(new AS::OperInstr(assm,nullptr,
            new TEMP::TempList(lefttemp,new TEMP::TempList(srctemp,nullptr)),nullptr));
          return;
        }
        //MOVE(MEM(BINOP(PLUS,CONST(i),e1)),e2)
        if(binopExp->op==T::PLUS_OP&&binopExp->left->kind==T::Exp::CONST)
        {
          TEMP::Temp *temp=TEMP::Temp::NewTemp();
          TEMP::Temp *righttemp=munchExp(binopExp->right);
          TEMP::Temp *srctemp=munchExp(src);
          assm=" movq `s1, ";
          emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
            new TEMP::TempList(righttemp,new TEMP::TempList(srctemp,nullptr)),nullptr));
          return;
        }
      }
    }
    if(src->kind==T::Exp::MEM)
    {
      //MOVE(MEM(e1), MEM(e2))
      T::MemExp *srcMem=(T::MemExp *)src;
      T::MemExp *dstMem=(T::MemExp*)dst;
      TEMP::Temp *srctemp=munchExp(srcMem);
      TEMP::Temp *dsttemp=munchExp(dstMem);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      assm="mov";
      emit(new AS::OperInstr(assm,nullptr,
            new TEMP::TempList(dsttemp,new TEMP::TempList(srctemp,nullptr)),nullptr));
      return;
    }
    if(dst->kind==T::Exp::MEM)
    {
      T::MemExp *dstMem=(T::MemExp*) dst;
      //MOVE(MEM(CONST(i)), e2)
      if(dstMem->exp->kind==T::Exp::CONST)
      {
        TEMP::Temp *srctemp=munchExp(src);
        assm=" ";
        emit(new AS::OperInstr(assm,nullptr,
            new TEMP::TempList(srctemp,nullptr),nullptr));
        return;
      }
      else{
        //MOVE(MEM(e1), e2)
        TEMP::Temp *e2=munchExp(src);
        TEMP::Temp *e1=munchExp(dstMem);
        assm=" ";
         emit(new AS::OperInstr(assm,nullptr,
            new TEMP::TempList(e1,new TEMP::TempList(e2,nullptr)),nullptr));
       return;
      }
    }
    else if(dst->kind==T::Exp::TEMP)
    {
      TEMP::Temp *e2=munchExp(src);
      T::TempExp *dsttempexp=(T::TempExp*) dst;
      assm="";
      emit(new AS::MoveInstr(assm,new TEMP::TempList(dsttempexp->temp,nullptr)
        ,new TEMP::TempList(e2,nullptr)));
      return;
    }
    break;
  
  
  }
  case T::Stm::JUMP:
  {
    T::JumpStm *jumpstm=(T::JumpStm*) stm;
    TEMP::LabelList *jumplist=jumpstm->jumps;
    assm="";
    emit(new AS::OperInstr(assm,nullptr,nullptr,new AS::Targets(jumplist)));
    return ;
  }
  case T::Stm::CJUMP:
  {
    T::CjumpStm *cjumpstm=(T::CjumpStm *)stm;
    TEMP::Temp *left=munchExp(cjumpstm->left);
    TEMP::Temp *right=munchExp(cjumpstm->right);
    TEMP::Label *trues=cjumpstm->true_label;
    TEMP::Label *falses=cjumpstm->false_label;

    assm="";
    emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(left,
      new TEMP::TempList(right,nullptr)),nullptr));

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

    //
    emit(new AS::OperInstr(assm,nullptr,nullptr,new AS::Targets(
      new TEMP::LabelList(trues,new TEMP::LabelList(falses,nullptr)))));
    return;
  }
  case T::Stm::LABEL:
  {
    T::LabelStm *labelstm=(T::LabelStm *)stm;
    emit(new AS::LabelInstr(TEMP::LabelString(labelstm->label),labelstm->label));
    break;
  }
  default:
    break;
  }
}

static TEMP::Temp *munchExp(T::Exp *exp)
{
  std::string assm;
  switch (exp->kind)
  {
  case T::Exp::MEM:
  {
    T::MemExp *memexp=(T::MemExp*) exp;
    if(memexp->exp->kind==T::Exp::BINOP)
    {
      T::BinopExp *binopexp=(T::BinopExp*) memexp;
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      //MEM(BINOP(PLUS,e1,CONST(i)))
      if(binopexp->op==T::PLUS_OP&& binopexp->right->kind==T::Exp::CONST)
      {
        T::Exp *e1=binopexp->left;
        TEMP::Temp *e1munch=munchExp(e1);
        assm="mov ";
        emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(e1munch,nullptr),nullptr));
        return temp;
      }
      //MEM(BINOP(PLUS,CONST(i),e1))
      if(binopexp->op==T::PLUS_OP&&binopexp->left->kind==T::Exp::CONST)
      {
        T::Exp *e1=binopexp->right;
        TEMP::Temp *e1munch=munchExp(e1);
        assm="mov ";
        emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(e1munch,nullptr),nullptr));
        return temp;
      }
    }
    //MEM(CONST(i))
    if(memexp->exp->kind==T::Exp::CONST)
    {
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      T::ConstExp *constexp=(T::ConstExp *)memexp->exp;
      //assm=constexp->consti;
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),nullptr,nullptr));
      return temp;
    }
    else{
      //MEM(e1)
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      TEMP::Temp *munche1=munchExp(exp);
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),new TEMP::TempList(munche1,nullptr),nullptr));
      return temp;
    }
    break;
  }
  case T::Exp::BINOP:
  {
    T::BinopExp *binopexp=(T::BinopExp*) exp;
    //BINOP(PLUS,e1,CONST(i))
    if(binopexp->op==T::PLUS_OP&&binopexp->right->kind==T::Exp::CONST);
    {
      T::Exp *e1=binopexp->left;
      TEMP::Temp *e1munch=munchExp(e1);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      assm="";
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),new TEMP::TempList(e1munch,nullptr),nullptr));
      return temp;
    }
    //BINOP(PLUS,CONST(i),e1)
    if(binopexp->op==T::PLUS_OP&&binopexp->left->kind==T::Exp::CONST)
    {
      T::Exp *e1=binopexp->right;
      TEMP::Temp *e1munch=munchExp(e1);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      assm="mov ";
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
      new TEMP::TempList(e1munch,nullptr),nullptr));
      return temp;
    }
    //BINOP(PLUS,e1,e2)
    if(binopexp->op==T::PLUS_OP)
    {
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      T::Exp *e1=binopexp->left;
      T::Exp *e2=binopexp->right;
      TEMP::Temp *e1munch=munchExp(binopexp->left);
      TEMP::Temp *e2munch=munchExp(binopexp->right);
      assm=" ";
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),new TEMP::TempList(e1munch,
        new TEMP::TempList(e2munch,nullptr)),nullptr));
      return temp;
    }
  
  
  }
  //CONST(i)
  case T::Exp::CONST:
  {
      TEMP::Temp *e1munch=munchExp(exp);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      assm="mov ";
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
      new TEMP::TempList(e1munch,nullptr),nullptr));
      return temp;
  }
  //TEMP(t)
  case T::Exp::TEMP:
  {
    T::TempExp *tempexp=(T::TempExp*) exp;
    return tempexp->temp;
  }
  case T::Exp::NAME:
  {
    TEMP::Temp *temp=TEMP::Temp::NewTemp();
    assm="  ";
    emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),nullptr,nullptr));
    return temp;
  }
  //
  case T::Exp::ESEQ:
  {
    T::EseqExp *eseqexp=(T::EseqExp *)exp;
    munchStm(eseqexp->stm);
    return munchExp(eseqexp->exp);
  }

  case T::Exp::CALL:
  {
    return nullptr;
  }
  default:
    assert(0);
    return nullptr;
    break;
  }
  assert(0);
}





}  // namespace CG