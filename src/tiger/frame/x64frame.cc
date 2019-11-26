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
      F::InRegAccess *regAccess=(F::InRegAccess *)access;
      return new T::TempExp(regAccess->reg);
    }
    else{
      F::InFrameAccess *frame_access=(F::InFrameAccess*)access;
      fprintf(stderr,"%d\n",frame_access->offset);
      assert(fp);
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
    F::Frame *new_frame=new F::Frame(makeFormals(boollist,wordSize),name,0);
    return new_frame;
  }

  Access *allocLocal(Frame *frame,bool escape)
  {
    int offset;
    TEMP::Temp *tempReg;
    if(escape==true)
    {
      frame->size++;
      offset=frame->size*wordSize;
      return new InFrameAccess(offset);
    }
    else{
      return new InRegAccess(tempReg);
    }
  }


  TEMP::Temp *FP(void) 
  {
    return TEMP::Temp::NewTemp();
  }

  TEMP::Temp *SP(void)
  {
    return TEMP::Temp::NewTemp();
  }

  TEMP::Temp *RV(void)
  {
    return TEMP::Temp::NewTemp();
  }

  TEMP::Temp *Zero(void)
  {
    return TEMP::Temp::NewTemp();
  }

  T::Exp *externalCall(std::string s,T::ExpList *args)
  {
    return new T::CallExp(new T::NameExp(TEMP::NamedLabel(s)),
      new T::ExpList(new T::ConstExp(0),args));
  }
}  // namespace F