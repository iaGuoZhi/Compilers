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
  AS::InstrList *instrlist;
  T::StmList *stmlist=stmList;
  //fprintf(stdout,"------------code generation-----------\n");
  //save callee saved registers
  TEMP::Temp *rbx=TEMP::Temp::NewTemp();
  TEMP::Temp *rsi=TEMP::Temp::NewTemp();
  TEMP::Temp *rdi=TEMP::Temp::NewTemp();

  emit(new AS::MoveInstr("movq `s0 `d0\n",new TEMP::TempList(rbx,nullptr),new TEMP::TempList(F::RBX(),nullptr)));
  emit(new AS::MoveInstr("movq `s0 `d0\n",new TEMP::TempList(rsi,nullptr),new TEMP::TempList(F::RSI(),nullptr)));
  emit(new AS::MoveInstr("movq `s0 `d0\n",new TEMP::TempList(rdi,nullptr),new TEMP::TempList(F::RDI(),nullptr)));

  for(stmlist;stmlist;stmlist=stmlist->tail)
  {
    munchStm(stmlist->head);
  }

  //restore callee saved registers
  emit(new AS::MoveInstr("movq `s0 `d0\n",new TEMP::TempList(F::RBX(),nullptr),new TEMP::TempList(rbx,nullptr)));
  emit(new AS::MoveInstr("movq `s0 `d0\n",new TEMP::TempList(F::RSI(),nullptr),new TEMP::TempList(rsi,nullptr)));
  emit(new AS::MoveInstr("movq `s0 `d0\n",new TEMP::TempList(F::RDI(),nullptr),new TEMP::TempList(rdi,nullptr)));

  instrlist=iList;
  return instrlist;
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
      assm="movq$ `s0, `d0";
      emit(new AS::MoveInstr(assm,new TEMP::TempList(dsttempexp->temp,nullptr)
        ,new TEMP::TempList(e2,nullptr)));
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
      emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(srctemp,
        new TEMP::TempList(memtemp,nullptr)),new AS::Targets(nullptr)));
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
    sprintf(inst,"jmp %s",TEMP::LabelString(label));
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
    emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(right,
      new TEMP::TempList(left,nullptr)),nullptr));

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
    //
    emit(new AS::OperInstr(assm,nullptr,nullptr,new AS::Targets(
      new TEMP::LabelList(trues,nullptr))));
    return;
  }
  case T::Stm::LABEL:
  {
    //fprintf(stdout,"-----------------label stm--------------\n");
    T::LabelStm *labelstm=(T::LabelStm *)stm;
    assm=TEMP::LabelString(labelstm->label);
    assm+=":";
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
  if (explist)
  {
    //fprintf(stdout,"-----------------munchArgs--------------\n");
    TEMP::Temp *temp=TEMP::Temp::NewTemp();
    munchArgs(explist->tail);
    temp=munchExp(explist->head);
    emit(new AS::OperInstr("pushl `s0",nullptr,new TEMP::TempList(temp,nullptr),new AS::Targets(nullptr)));
  }
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
    emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
    new TEMP::TempList(memtmp,nullptr), new AS::Targets(nullptr)));
    return temp;
  }
  case T::Exp::BINOP:
  {
    //fprintf(stdout,"-----------------binop exp--------------\n");
    T::BinopExp *binopexp=(T::BinopExp*) exp;
    //movq left,dst
    //addq right dst
    if(binopexp->op==T::PLUS_OP)
    {
      TEMP::Temp *left=munchExp(binopexp->left);
      TEMP::Temp *right=munchExp(binopexp->right);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("addq `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(right,new TEMP::TempList(temp,nullptr)),new AS::Targets(nullptr)));
      return temp;
    }
    //movq left,dst
    //subq right,dst
    if(binopexp->op==T::MINUS_OP)
    {
      TEMP::Temp *left=munchExp(binopexp->left);
      TEMP::Temp *right=munchExp(binopexp->right);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("subq `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(right,new TEMP::TempList(temp,nullptr)),new AS::Targets(nullptr)));
      return temp;
    }

    //movq left,dst
    //imul right,dst
    if(binopexp->op==T::MUL_OP)
    {
      TEMP::Temp *left=munchExp(binopexp->left);
      TEMP::Temp *right=munchExp(binopexp->right);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("imul `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(right,new TEMP::TempList(temp,nullptr)),new AS::Targets(nullptr)));
      return temp;
    }

     //movq left,%rax
     //cltd #use rax's sign bit to fill rdx
     //idivl right 
    if(binopexp->op==T::DIV_OP)
    {
      TEMP::Temp *left=munchExp(binopexp->left);
      TEMP::Temp *right=munchExp(binopexp->right);
      TEMP::Temp *temp=TEMP::Temp::NewTemp();
      emit(new AS::MoveInstr("movl `s0, `d0",new TEMP::TempList(F::RAX(),nullptr),new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("cltd",new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr))
      ,new TEMP::TempList(F::RAX(),nullptr), new AS::Targets(nullptr)));
      emit(new AS::OperInstr("idivl `s0",new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr)),
        new TEMP::TempList(right,new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr))),
          new AS::Targets(nullptr)));
      emit(new AS::MoveInstr("movl `s0, `d0",new TEMP::TempList(temp,nullptr),
        new TEMP::TempList(F::RAX(),nullptr)));
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
      emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),
      nullptr,nullptr));
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
    sprintf(inst,"movq $%s, `d0",TEMP::LabelString(nameexp->name));
    assm=std::string(inst);
    emit(new AS::OperInstr(assm,new TEMP::TempList(temp,nullptr),nullptr,nullptr));
    return temp;
  }
  //
  case T::Exp::ESEQ:
  {
    /*//fprintf(stdout,"-----------------eseq exp--------------\n");
    T::EseqExp *eseqexp=(T::EseqExp *)exp;
    munchStm(eseqexp->stm);
    return munchExp(eseqexp->exp);*/
    return nullptr;
  }

  case T::Exp::CALL:
  {
    //fprintf(stdout,"-----------------call exp--------------\n");
    T::CallExp *callexp=(T::CallExp *)exp;
    TEMP::Temp *temp=TEMP::Temp::NewTemp();
    munchArgs(callexp->args);
    TEMP::Label *label=((T::NameExp *)callexp->fun)->name;
    sprintf(inst,"call %s",TEMP::LabelString(label));
    assm=std::string(inst);
    emit(new AS::OperInstr(assm,F::Callersaves(),nullptr,new AS::Targets(nullptr)));
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