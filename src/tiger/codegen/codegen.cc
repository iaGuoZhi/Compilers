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
std::map<int,int> mapping;


int mapping_stack_temp(int temp_int)
{
  if(mapping.find(temp_int)==mapping.end())
  {
    max_offset+=wordSize;
    int offset=max_offset;
    mapping[temp_int]=offset;
  }
  return mapping[temp_int];
}

void store_temp_into_stack(TEMP::Temp *temp)
{
  //the special reg to store stack top position
  if(temp->Int()==100)
  {
    layermap->Enter(temp,new std::string("%r15"));
    return;
  }
  std::stringstream stream;
  stream<<mapping_stack_temp(temp->Int())<<"(%rsp)";
  layermap->Enter(temp,new std::string(stream.str()));
}

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
void make_link(int size)
{
  AS::OperInstr *leaq;
  char inst[MAX_INST_LEN];
  sprintf(inst,"leaq %d(%%rsp), %%r15",size);
  leaq=new AS::OperInstr(std::string(inst),nullptr,nullptr,nullptr);
  iList=new AS::InstrList(leaq,iList);
}

RA::Result Codegen(F::Frame* f, T::StmList* stmList) {
  //init
  iList=nullptr;
  iLast=nullptr;
  layermap=TEMP::Map::Empty();
  F::add_register_to_map(layermap);
  // TODO: Put your codes here (lab6).
  RA::Result allocation;
  AS::InstrList *instrlist;
  T::StmList *stmlist=stmList;
  framesize=f->size*wordSize;
  func_label=f->label;
  for(stmlist;stmlist;stmlist=stmlist->tail)
  {
    munchStm(stmlist->head);
  }

  framesize=framesize+max_offset+wordSize;
  f->size=framesize/wordSize;
  //保护在tr中被用到的寄存器名字
  layermap->Enter(F::RAX(),new std::string("%rax"));
  make_link(framesize);
  allocation.il=iList;
  allocation.coloring=layermap;
  return allocation;
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

      store_temp_into_stack(e2);
      store_temp_into_stack(dsttempexp->temp);
     
      assm="movq `s0, `d0";

      //load from memory
      emit(new  AS::MoveInstr(assm,new TEMP::TempList(F::R12(),nullptr),new TEMP::TempList(e2,nullptr)));
      //store to memory
      emit(new AS::MoveInstr(assm,new TEMP::TempList(dsttempexp->temp,nullptr),new TEMP::TempList(F::R12(),nullptr)));
      return;
    }
    //move src,mem
    if(dst->kind==T::Exp::MEM)
    {
      //fprintf(stdout,"-----------movestm------------\n");
      TEMP::Temp *srctemp=munchExp(src);
      T::MemExp *memexp=(T::MemExp*) dst;
      TEMP::Temp *memtemp=munchExp(memexp->exp);
     
      store_temp_into_stack(srctemp);
      store_temp_into_stack(memtemp);

      assm="movq `s0, `d0";
      emit(new AS::OperInstr(assm,new TEMP::TempList(F::R13(),nullptr),new TEMP::TempList(memtemp,nullptr),nullptr));
      emit(new AS::OperInstr(assm,new TEMP::TempList(F::R12(),nullptr),new TEMP::TempList(srctemp,nullptr),nullptr));
      assm="movq `s0, (`s1)";
      emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(F::R12(),new TEMP::TempList(F::R13(),nullptr)),new AS::Targets(nullptr)));
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

    store_temp_into_stack(left);
    store_temp_into_stack(right);

    assm="movq `s0, `d0";
    emit(new AS::OperInstr(assm,new TEMP::TempList(F::R13(),nullptr),new TEMP::TempList(left,nullptr),nullptr));
    emit(new AS::OperInstr(assm,new TEMP::TempList(F::R12(),nullptr),new TEMP::TempList(right,nullptr),nullptr));


    assm="cmp `s0, `s1";
    emit(new AS::OperInstr(assm,nullptr,new TEMP::TempList(F::R12(),new TEMP::TempList(F::R13(),nullptr)),nullptr));
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
    
    store_temp_into_stack(temp);
    store_temp_into_stack(memtmp);

    assm="movq `s0, `d0";
    emit(new AS::OperInstr(assm,new TEMP::TempList(F::R13(),nullptr),new TEMP::TempList(memtmp,nullptr),nullptr));
    assm="movq (`s0), `d0";
    emit(new AS::OperInstr(assm,new TEMP::TempList(F::R12(),nullptr),new TEMP::TempList(F::R13(),nullptr), new AS::Targets(nullptr)));
    assm="movq `s0, `d0";
    emit(new AS::MoveInstr(assm,new TEMP::TempList(temp,nullptr),new TEMP::TempList(F::R12(),nullptr)));
    return temp;
  }
  case T::Exp::BINOP:
  {
    //fprintf(stdout,"-----------------binop exp--------------\n");
    T::BinopExp *binopexp=(T::BinopExp*) exp;
    TEMP::Temp *left=munchExp(binopexp->left);
    TEMP::Temp *right=munchExp(binopexp->right);
    TEMP::Temp *temp=TEMP::Temp::NewTemp();

    store_temp_into_stack(left);
    store_temp_into_stack(right);
    store_temp_into_stack(temp);

    //movq left,dst
    //addq right dst
    if(binopexp->op==T::PLUS_OP)
    { 
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R12(),nullptr), new TEMP::TempList(left,nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R13(),nullptr), new TEMP::TempList(right,nullptr)));
      emit(new AS::OperInstr("addq `s0, `d0",new TEMP::TempList(F::R12(),nullptr), new TEMP::TempList(F::R13(),nullptr),new AS::Targets(nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(F::R12(),nullptr)));
      return temp;
    }
    //movq left,dst
    //subq right,dst
    if(binopexp->op==T::MINUS_OP)
    {
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R12(),nullptr), new TEMP::TempList(left,nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R13(),nullptr), new TEMP::TempList(right,nullptr)));
      emit(new AS::OperInstr("subq `s0, `d0",new TEMP::TempList(F::R12(),nullptr), new TEMP::TempList(F::R13(),nullptr),new AS::Targets(nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(F::R12(),nullptr)));
      return temp;
    }

    //movq left,dst
    //imul right,dst
    if(binopexp->op==T::MUL_OP)
    {
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R12(),nullptr), new TEMP::TempList(left,nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R13(),nullptr), new TEMP::TempList(right,nullptr)));
      emit(new AS::OperInstr("imul `s0, `d0",new TEMP::TempList(F::R12(),nullptr), new TEMP::TempList(F::R13(),nullptr),new AS::Targets(nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr), new TEMP::TempList(F::R12(),nullptr)));
       return temp;
    }

     //movq left,%RAX
     //cltd #use RAX's sign bit to fill rdx
     //idivl right 
    if(binopexp->op==T::DIV_OP)
    {
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::R13(),nullptr),new TEMP::TempList(right,nullptr)));
      emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(F::RAX(),nullptr),new TEMP::TempList(left,nullptr)));
      emit(new AS::OperInstr("cltd",nullptr,nullptr, new AS::Targets(nullptr)));
      emit(new AS::OperInstr("idivq `s0",new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr)),
        new TEMP::TempList(F::R13(),new TEMP::TempList(F::RDX(),new TEMP::TempList(F::RAX(),nullptr))),new AS::Targets(nullptr)));
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
      store_temp_into_stack(temp);

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
    store_temp_into_stack(tempexp->temp);
    return tempexp->temp;
  }
  case T::Exp::NAME:
  {
    //fprintf(stdout,"-----------------name exp--------------\n");
    T::NameExp *nameexp=(T::NameExp*) exp;
    TEMP::Temp *temp=TEMP::Temp::NewTemp();

    store_temp_into_stack(temp);
    
    sprintf(inst, "leaq %s(%%rip), `d0", TEMP::LabelString(nameexp->name).c_str());
    assm=std::string(inst);
    emit(new AS::OperInstr(assm,new TEMP::TempList(F::R12(),nullptr),nullptr,nullptr));
    
    emit(new AS::MoveInstr("movq `s0, `d0",new TEMP::TempList(temp,nullptr),new TEMP::TempList(F::R12(),nullptr)));
    return temp;
  }
  

  case T::Exp::CALL:
  {
    //fprintf(stdout,"-----------------call exp--------------\n");
    T::CallExp *callexp=(T::CallExp *)exp;
    TEMP::Temp *temp=TEMP::Temp::NewTemp();

    store_temp_into_stack(temp);
    munchArgs(callexp->args);
    TEMP::Label *label=((T::NameExp *)callexp->fun)->name;
    sprintf(inst,"call %s",TEMP::LabelString(label).c_str());
    assm=std::string(inst);
    emit(new AS::OperInstr(assm,nullptr,nullptr,new AS::Targets(nullptr)));
    //recover rsp point
    while(rspoffset>0)
    {
      rspoffset-=wordSize;
      emit(new AS::OperInstr("popq `d0",new TEMP::TempList(F::R12(),nullptr),nullptr,new AS::Targets(nullptr)));
    }
    sprintf(inst,"leaq %s_framesize(%%rsp), %%r15",func_label->Name().c_str());
    emit(new AS::OperInstr(std::string(inst),nullptr,nullptr,new AS::Targets(nullptr)));
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