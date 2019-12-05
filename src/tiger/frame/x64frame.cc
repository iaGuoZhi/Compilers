#include "tiger/frame/frame.h"

#include <string>

namespace F {

  class X64Frame : public Frame {
    // TODO: Put your codes here (lab6).

  };

  /*Frame*/
  Frame::Frame(AccessList *formal,TEMP::Label *label,int size)
  {
    this->formal=formal;
    this->label=label;
    this->size=size;
  }
  class AccessList *Frame::getFormals()
  {
    return this->formal;
  }

  TEMP::Label *Frame::getLabel()
  {
    return this->label;
  }

  class InFrameAccess : public Access {
  public:
    int offset;
    int getOffset()
    {
      return this->offset;
    }
    InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}
  };

  class InRegAccess : public Access {
  public:
    TEMP::Temp* reg;

    InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {}
  };

  T::Exp *Exp(Access *access,T::Exp *fp)
  {
    if(access->kind==F::Access::INREG)
    { 
      assert(0);
      F::InRegAccess *regAccess=(F::InRegAccess *)access;
      return new T::TempExp(regAccess->reg);
    }
    else{
      F::InFrameAccess *frame_access=(F::InFrameAccess*)access;
      fprintf(stderr,"%d\n",frame_access->offset);
      assert(fp);
      //assert(((F::InFrameAccess*)access)->offset<100);
      return new T::MemExp(new T::BinopExp(
        T::PLUS_OP,fp,new T::ConstExp(((F::InFrameAccess*)access)->offset)
      ));
    }
  }

  static class AccessList *makeFormals(U::BoolList *boolist,int offset)
  {
    if(boolist!=nullptr)
    {
      return new AccessList(new InFrameAccess(offset),makeFormals(boolist->tail,offset+wordSize));
    }
    return nullptr;
  }

  F::Frame *newFrame(TEMP::Label *name,U::BoolList *boollist)
  {
    /*F::Frame *new_frame=new F::Frame(new AccessList(nullptr,nullptr),name,12);
    F::AccessList *tail=new_frame->formal;
    int count=0;
    for(;boollist;boollist=boollist->tail)
    {
      bool escape=boollist->head;
      Access *new_access;
      if(escape){
        ++count;
        new_access=new InFrameAccess((p1+count)*wordSize);
      }else{
        assert(0);
        new_access=new InRegAccess(TEMP::Temp::NewTemp());
      }
      tail->tail=new AccessList(new_access,nullptr);
      tail=tail->tail;
    }
    new_frame->formal=new_frame->formal->tail;
    //F::Frame *new_frame=new F::Frame(makeFormals(boollist,wordSize),name,0);
    return new_frame;*/
    AccessList *formals=makeFormals(boollist,8);
    F::Frame *new_frame=new F::Frame(formals,name,0);
  }

  Access *allocLocal(Frame *frame,bool escape)
  {
    int offset;
    TEMP::Temp *tempReg=TEMP::Temp::NewTemp();
    if(escape==true)  
    {
      frame->size++;
      assert(wordSize==8);
      offset=-(frame->size*wordSize);
      return new InFrameAccess(offset);
    }
    else{
      return new InRegAccess(tempReg);
    }
  }

  /*static F::AccessList *callerSavesAccess(F::Frame *frame)
  {
    TEMP::TempList *callers=Callersaves();
    AccessList *l=nullptr;
    AccessList *last=nullptr;

    for(; callers; callers=callers->tail)
    {
      F::Access *access=F::allocLocal(frame,false);

      if(!last){
        last=new AccessList(access,nullptr);
        l=last;
      }else{
        last->tail=new AccessList(access,nullptr);
        last=last->tail;
      }
    }
    return l;
  }*/

  



  /****************************************************/
  static TEMP::Map *specialregs=nullptr;
  static TEMP::Map *argregs=nullptr;
  static TEMP::Map *calleesaves=nullptr;
  static TEMP::Map *callersaves=nullptr;

  /***************************************************/
  TEMP::Temp *RSP(void){
    static TEMP::Temp *rsp=nullptr;
    if(!rsp){
      rsp=TEMP::Temp::NewTemp();
    }
    return rsp;
  }

  TEMP::Temp *RAX(void){
    static TEMP::Temp *rax=nullptr;
    if(!rax){
      rax=TEMP::Temp::NewTemp();
    }
    return rax;
  }
  //rbp,rbx,rdi,rdx,rcx,r8,r9,r10,r11,r12,r13,r14,r15
  TEMP::Temp *RBP(void){
    static TEMP::Temp *rbp=nullptr;
    if(!rbp){
      rbp=TEMP::Temp::NewTemp();
    }
    return rbp;
  }

  TEMP::Temp *RBX(void){
    static TEMP::Temp *rbx=nullptr;
    if(!rbx)
    {
      rbx=TEMP::Temp::NewTemp();
    }
    return rbx;
  }

  TEMP::Temp *RDI(void){
    static TEMP::Temp *rdi=nullptr;
    if(!rdi){
      rdi=TEMP::Temp::NewTemp();
    }
    return rdi;
  }

  TEMP::Temp *RSI(void){
    static TEMP::Temp *rsi=nullptr;
    if(!rsi){
      rsi=TEMP::Temp::NewTemp();
    }
    return rsi;
  }
  TEMP::Temp *RDX(void)
  {
    static TEMP::Temp *rdx=nullptr;
    if(!rdx){
      rdx=TEMP::Temp::NewTemp();
    }
    return rdx;
  }

  TEMP::Temp *RCX(void)
  {
    static TEMP::Temp *rcx=nullptr;
    if(!rcx){
      rcx=TEMP::Temp::NewTemp();
    }
    return rcx;
  }

  TEMP::Temp *R8(void)
  {
    static TEMP::Temp *r8=nullptr;
    if(!r8){
      r8=TEMP::Temp::NewTemp();
    }
    return r8;
  }

  TEMP::Temp *R9(void)
  {
    static TEMP::Temp *r9=nullptr;
    if(!r9){
      r9=TEMP::Temp::NewTemp();
    }
    return r9;
  }

  TEMP::Temp *R10(void)
  {
    static TEMP::Temp *r10=nullptr;
    if(!r10){
      r10=TEMP::Temp::NewTemp();
    }
    return r10;
  }

  TEMP::Temp *R11(void)
  {
    static TEMP::Temp *r11=nullptr;
    if(!r11){
      r11=TEMP::Temp::NewTemp();
    }
    return r11;
  }

  TEMP::Temp *R12(void)
  {
    static TEMP::Temp *r12=nullptr;
    if(!r12){
      r12=TEMP::Temp::NewTemp();
    }
    return r12;
  }

  TEMP::Temp *R13(void)
  {
    static TEMP::Temp *r13=nullptr;
    if(!r13){
      r13=TEMP::Temp::NewTemp();
    }
    return r13;
  }

  TEMP::Temp *R14(void)
  {
    static TEMP::Temp *r14=nullptr;
    if(!r14){
      r14=TEMP::Temp::NewTemp();
    }
    return r14;
  }

  TEMP::Temp *R15(void)
  {
    static TEMP::Temp *r15=nullptr;
    if(!r15){
      r15=TEMP::Temp::NewTemp();
    }
    return r15;
  }

  TEMP::Temp *RV(void)
  {
    return RAX();
  }
  TEMP::Temp *FP(void) 
  {
    
    return R15();
  }

  TEMP::Temp *SP(void)
  {
    return RSP();
  }

  /*******************************************************/
/*
  TEMP::TempList *SpecialRegs()
  {
    static TEMP::TempList *regs=nullptr;
    if(regs==nullptr)
    {
      regs=new TEMP::TempList(SP(),new TEMP::TempList(RV(),new TEMP::TempList(FP(),nullptr)));
    }
    return regs;
  }
*/
  TEMP::Temp *Args(int index){
    switch (index)
    {
    case 0: return RDI();
    case 1: return RSI();
    case 2: return RDX();
    case 3: return RCX();
    case 4: return R8();
    case 5: return R9();   
    default:
      assert(0);
    }
    return nullptr;
  }

  TEMP::TempList *ArgRegs(void)
  {
    static TEMP::TempList *argregs=nullptr;
    if(argregs==nullptr){
    argregs=new TEMP::TempList(Args(0),
            new TEMP::TempList(Args(1),
            new TEMP::TempList(Args(2),
            new TEMP::TempList(Args(3),
            new TEMP::TempList(Args(4),
            new TEMP::TempList(Args(5),
            nullptr))))));
    }
    return argregs;
  }

  void add_register_to_map(TEMP::Map *frameMap)
  {
    //specialregs: %rsp %rax
    frameMap->Enter(FP(),new std::string("%rsp"));
    frameMap->Enter(SP(),new std::string("%rsp"));
    frameMap->Enter(RV(),new std::string("%rax"));

    //argregs: %rdi %rsi %rdx %rcx %r8 %r9
    frameMap->Enter(RDI(),new std::string("%rdi"));
    frameMap->Enter(RSI(),new std::string("%rsi"));
    frameMap->Enter(RDX(),new std::string("%rdx"));
    frameMap->Enter(RCX(),new std::string("%rcx"));
    frameMap->Enter(R8(),new std::string("%r8"));
    frameMap->Enter(R9(),new std::string("%r9"));

    //callersaveregs: %r10 %r11
    frameMap->Enter(R10(),new std::string("%r10"));
    frameMap->Enter(R11(),new std::string("%r11"));

    //calleesaveregs : %rbx %rbp %r12 %r13 %r14 %r15
    frameMap->Enter(RBX(),new std::string("%rbx"));
    frameMap->Enter(RBP(),new std::string("%rbp"));
    frameMap->Enter(R12(),new std::string("%r12"));
    frameMap->Enter(R13(),new std::string("%r13"));
    frameMap->Enter(R14(),new std::string("%r14"));
    frameMap->Enter(R15(),new std::string("%r15"));

  }
/*
  TEMP::TempList *Calleesaves()
  {
    static TEMP::TempList *regs=nullptr;
    if(regs=nullptr)
    {
      regs=new TEMP::TempList(RBX(),
           new TEMP::TempList(RBP(),
           new TEMP::TempList(R12(),
           new TEMP::TempList(R13(),
           new TEMP::TempList(R14(),
           new TEMP::TempList(R15(),
           nullptr))))));
    }
    return regs;
  }

  TEMP::TempList *Callersaves()
  {
    static TEMP::TempList *regs=nullptr;
    if(regs=nullptr)
    {
      regs=new TEMP::TempList(R10(),
           new TEMP::TempList(R11(),
           nullptr));
    }
    return regs;
  }

  TEMP::TempList *Registers()
  {
    static TEMP::TempList *registers=nullptr;
    if(registers==nullptr)
    {
      registers=new TEMP::TempList(RV(),
                new TEMP::TempList(RDI(),
                new TEMP::TempList(RSI(),
                new TEMP::TempList(RDX(),
                new TEMP::TempList(RCX(),
                new TEMP::TempList(R8(),
                new TEMP::TempList(R9(),
                new TEMP::TempList(R10(),
                new TEMP::TempList(R11(),
                new TEMP::TempList(R12(),
                new TEMP::TempList(R13(),
                new TEMP::TempList(R14(),
                new TEMP::TempList(R15(),
                new TEMP::TempList(RSP(),
                new TEMP::TempList(RBP(),
                new TEMP::TempList(RBX(),
                nullptr))))))))))))))));
    }
    return registers;
  }

  void InitTempMap()
  {
    //specialregs: %rsp %rax
    frameMap->Enter(FP(),new std::string("%rsp"));
    frameMap->Enter(SP(),new std::string("%rsp"));
    frameMap->Enter(RV(),new std::string("%rax"));

    //argregs: %rdi %rsi %rdx %rcx %r8 %r9
    frameMap->Enter(RDI(),new std::string("%rdi"));
    frameMap->Enter(RSI(),new std::string("%rsi"));
    frameMap->Enter(RDX(),new std::string("%rdx"));
    frameMap->Enter(RCX(),new std::string("%rcx"));
    frameMap->Enter(R8(),new std::string("%r8"));
    frameMap->Enter(R9(),new std::string("%r9"));

    //callersaveregs: %r10 %r11
    frameMap->Enter(R10(),new std::string("%r10"));
    frameMap->Enter(R11(),new std::string("%r11"));

    //calleesaveregs : %rbx %rbp %r12 %r13 %r14 %r15
    frameMap->Enter(RBX(),new std::string("%rbx"));
    frameMap->Enter(RBP(),new std::string("%rbp"));
    frameMap->Enter(R12(),new std::string("%r12"));
    frameMap->Enter(R13(),new std::string("%r13"));
    frameMap->Enter(R14(),new std::string("%r14"));
    frameMap->Enter(R15(),new std::string("%r15"));

    //initialize specialregs
    if(specialregs==nullptr)
    {
      specialregs=TEMP::Map::Empty();
      specialregs->Enter(FP(),new std::string("%rbp"));
      specialregs->Enter(RV(),new std::string("%rax"));
      specialregs->Enter(SP(),new std::string("%rsp"));
    }

    //initialize argregs
    if(argregs==nullptr)
    {
      argregs=TEMP::Map::Empty();
      argregs->Enter(RDI(),new std::string("%rdi"));
      argregs->Enter(RSI(),new std::string("%rsi"));
      argregs->Enter(RDX(),new std::string("%rdx"));
      argregs->Enter(RCX(),new std::string("%rcx"));
      argregs->Enter(R8(),new std::string("%r8"));
      argregs->Enter(R9(),new std::string("%r9"));
    }

    //initialize calleesaveregs
    if(calleesaves==nullptr)
    {
      calleesaves=TEMP::Map::Empty();
      calleesaves->Enter(RBP(),new std::string("%rbp"));
      calleesaves->Enter(RBX(),new std::string("%rbx"));
      calleesaves->Enter(R12(),new std::string("%r12"));
      calleesaves->Enter(R13(),new std::string("%r13"));
      calleesaves->Enter(R14(),new std::string("%r14"));
      calleesaves->Enter(R15(),new std::string("%r15"));
    }

    //initialize callersaveregs
    if(calleesaves==nullptr)
    {
      callersaves=TEMP::Map::Empty();
      callersaves->Enter(R10(),new std::string("%r10"));
      callersaves->Enter(R11(),new std::string("%r11"));
    }
    
  }
  TEMP::Temp *Zero(void)
  {
    return TEMP::Temp::NewTemp();
  }
*/
  /*********************************************************/
 
  char *prolog(F::Frame *frame)
  {
    char *out;
    sprintf(out,"pushl %%ebp\nmovl %%esp, %%ebp\nsubl $%d, %%esp\n", frame->size);
    return out;
  }

  char *epilog(F::Frame *frame)
  {
    char *out;
    sprintf(out,"reet\n");
    return out;
  }

   /* T::Stm *procEntryExit1(Frame *frame,T::Stm *stm)
  {
    return stm;
  }

  AS::InstrList *procEntryExit2(AS::InstrList *body)
  {
    static TEMP::TempList *returnSink=new TEMP::TempList(
      SP(),new TEMP::TempList(FP(),nullptr)
    );
    return body->Splice(body,new AS::InstrList(new AS::OperInstr(std::string("#exit2"),
        nullptr,returnSink,nullptr),nullptr));
  }*/

  AS::Proc *procEntryExit3(F::Frame *frame,AS::InstrList *body)
  {
    char buf[100],buf2[100];
    sprintf(buf, 
                    "subq $%d, %%rsp\n",
                frame->size*wordSize);
    sprintf(buf2,"addq $%d,%%rsp\nret\n",frame->size*wordSize);
     return new AS::Proc(std::string(buf),body,std::string(buf2));
  }

  T::Exp *externalCall(std::string s,T::ExpList *args)
  {
    return new T::CallExp(new T::NameExp(TEMP::NamedLabel(s)),
      args);
  }
}  // namespace F