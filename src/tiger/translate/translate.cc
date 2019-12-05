#include "tiger/translate/translate.h"

#include <cstdio>
#include <set>
#include <string>


#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/temp.h"
#include "tiger/semant/semant.h"
#include "tiger/semant/types.h"
#include "tiger/util/util.h"

static F::FragList *frags;
static const int wordSize=8;
static TR::Level *outermost=nullptr;

/*
  * 在tiger编译器的语义分析阶段，transDec通过调用TR_newLevel为每一个函数创建一个新的“嵌套层”，
  * tr_newlevel则调用f_newfram建立一个新栈帧。
  * semant将这个嵌套层保存在该函数的funentry数据结构中，以便当它遇到一个函数调用时，能够将这个被调用
  * 函数的嵌套层传回给translate。
  * funentry 也需要该函数的机器代码入口的标号。
  * */
namespace TR {

//与Faccess不同,它必须知道与静态链的有关信息。
class Access {
 public:
  TR::Level *level;
  F::Access *access;
  //变量的层次level，和他的faccess组成的偶对。
  Access(TR::Level *level, F::Access *access) : level(level), access(access) {}
  //为了在函数f的栈帧中分配新的局部变量，语义分析阶段需要调用，esacpe为false，说明变量可以分配到寄存器中
  static Access *AllocLocal(TR::Level *level, bool escape);
};


class AccessList {
 public:
  TR::Access *head;
  TR::AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};

//transDec中可调用
class Level {
 public:
  F::Frame *frame;
  TR::Level *parent;
  TR::AccessList *formals;

  Level(F::Frame *frame, TR::Level *parent,TR::AccessList *formals=nullptr) 
  : frame(frame), parent(parent) ,formals(formals){}
  TR::AccessList *Formals() { 
    return this->formals;
  }

   TR::Level *NewLevel(TR::Level *parent, TEMP::Label *name,
                         U::BoolList *boollist)
                         {
                           Level *level=new Level(
                              F::newFrame(name,new U::BoolList(true,boollist)),
                             parent
                           );
                           level->formals=level->make_access_list(level->frame->getFormals()->tail);
                           
                           return level;
                         }
  TR::AccessList *make_access_list(F::AccessList *f_access_list)
  {
    if(f_access_list==nullptr)
    {
      return nullptr;
    }
    else{
      return new TR::AccessList(new TR::Access(this,f_access_list->head),make_access_list(f_access_list->tail));
    }
  }
  TR::Access *AllocLocal(bool escape)
  {
    return new TR::Access(this, F::allocLocal(this->frame,escape));
  }

  T::Exp *staticLink(TR::Level *destLevel)
  {
    T::Exp *fp=new T::TempExp( F::FP());
    TR::Level *currentLevel=this;
    fprintf(stderr,"staticlink\n");
    while(currentLevel&&currentLevel!=destLevel)
    {
      //fprintf(stdout,"$$$$$$$$$$$$$$$$$%s\n",currentLevel->frame->label->Name().c_str());
      //fprintf(stderr,"begin formals\n");
      assert(currentLevel->frame);
     // fp=new T::MemExp(new T::BinopExp(T::PLUS_OP,new T::ConstExp(F::wordSize),fp));
      fp=F::Exp(currentLevel->frame->getFormals()->head,fp);
      assert(currentLevel->parent);
      currentLevel=currentLevel->parent;
    }
    return fp;
  }
};

inline Access *Access::AllocLocal(TR::Level *level, bool escape) {
    return new Access(level,F::allocLocal(level->frame,escape));
}
//静态链是由translate负责的 
class PatchList {
 public:
  TEMP::Label **head;
  PatchList *tail;

  PatchList(TEMP::Label **head, PatchList *tail) : head(head), tail(tail) {}
};

void do_patch(PatchList *tList, TEMP::Label *label) {
  for (; tList; tList = tList->tail) *(tList->head) = label;
}

class Cx {
 public:
  PatchList *trues;
  PatchList *falses;
  T::Stm *stm;

  Cx(PatchList *trues, PatchList *falses, T::Stm *stm)
      : trues(trues), falses(falses), stm(stm) {}
};

class Exp {
 public:
  enum Kind { EX, NX, CX };

  Kind kind;

  Exp(Kind kind) : kind(kind) {}

  virtual T::Exp *UnEx() const = 0;
  virtual T::Stm *UnNx() const = 0;
  virtual Cx UnCx() const = 0;
};

class ExpAndTy {
 public:
  TR::Exp *exp;
  TY::Ty *ty;

  ExpAndTy(TR::Exp *exp, TY::Ty *ty) : exp(exp), ty(ty) {}
};

class ExExp : public Exp {
 public:
  T::Exp *exp;

  ExExp(T::Exp *exp) : Exp(EX), exp(exp) {}

  T::Exp *UnEx() const override {
    return this->exp;
  }
  T::Stm *UnNx() const override {
    return new T::ExpStm(this->exp);
  }
  Cx UnCx() const override {
    Cx *cx;
    
    cx->stm=new T::CjumpStm(T::NE_OP,exp,new T::ConstExp(0),nullptr,nullptr);
    cx->trues=new PatchList( &((T::CjumpStm *)(cx->stm))->true_label,nullptr);
    cx->falses=new PatchList(&((T::CjumpStm *)(cx->stm))->false_label,nullptr);
    return *cx;
  }
};

class NxExp : public Exp {
 public:
  T::Stm *stm;

  NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

  T::Exp *UnEx() const override {
    return new T::EseqExp(this->stm,new T::ConstExp(0));
  }
  T::Stm *UnNx() const override {
    return this->stm;
  }
  Cx UnCx() const override {
    assert(0);
  }
};

class CxExp : public Exp {
 public:
  Cx cx;

  CxExp(struct Cx cx) : Exp(CX), cx(cx) {}
  CxExp(PatchList *trues, PatchList *falses, T::Stm *stm)
      : Exp(CX), cx(trues, falses, stm) {}

  T::Exp *UnEx() const override {
    TEMP::Temp *r=TEMP::Temp::NewTemp();
    TEMP::Label *t=TEMP::NewLabel(),*f=TEMP::NewLabel();
    do_patch(this->cx.trues,t);
    do_patch(this->cx.falses,f);
    return new T::EseqExp(new T::MoveStm(new T::TempExp(r),new T::ConstExp(1)),
                    new T::EseqExp(this->cx.stm,
                    new T::EseqExp(new T::LabelStm(f),
                    new T::EseqExp(new T::MoveStm(new T::TempExp(r),new T::ConstExp(0)),
                    new T::EseqExp(new T::LabelStm(t),new T::TempExp(r))))));
  }
  T::Stm *UnNx() const override {
    TEMP::Label *empty=TEMP::NewLabel();
    do_patch(this->cx.trues,empty);
    do_patch(this->cx.falses,empty);
    return new T::SeqStm(this->cx.stm,new T::LabelStm(empty));
  }
  Cx UnCx() const override {
    return this->cx;
  }
};

class ExpList {
 public:
  TR::Exp *head;
  TR::ExpList *tail;

  ExpList(Exp *head, ExpList *tail) : head(head), tail(tail) {}
};


PatchList *join_patch(PatchList *first, PatchList *second) {
  if (!first) return second;
  for (; first->tail; first = first->tail)
    ;
  first->tail = second;
  return first;
}

Level *Outermost() {
  if(outermost){
    return outermost;
  }
  else{
    F::Frame *frame=F::newFrame( TEMP::NamedLabel("tigermain"),nullptr);
    outermost=new TR::Level(frame,nullptr);
  }
}
/*T::Stm *Tr_procEntryExit(TR::Level *level,TR::Exp *body,TR::AccessList *formals)
{
  //transmit body value to RV register
  T::Stm *fragStm=new T::MoveStm(new T::TempExp(F::RV()),body->UnEx());
  F::Frag new_frag=new F::ProcFrag(F::procEntryExit1(fragStm,level->frame));
  frags=new F::FragList(&new_frag,frags);
}*/
T::Stm *procEntryExit(TR::Level *level,TR::Exp *body,TR::AccessList *formals)
{
  //transmit body value to RV register
  T::Stm *fragStm=new T::MoveStm(new T::TempExp(F::RAX()),body->UnEx());
  //F::Frag new_frag=F::ProcFrag(F::procEntryExit1(fragStm,level->frame);
  //frags=new F::FragList(&new_frag,frags);
  return fragStm;
}
F::FragList *getResult(){
  return frags;
}
void Func(TR::Exp *body,TR::Level *level)
{
  T::Stm *stm=TR::procEntryExit(level,body,level->formals);
  F::Frag *head=new F::ProcFrag(stm,level->frame);
  frags=new F::FragList(head,frags);
}

F::FragList *TranslateProgram(A::Exp *root) {
  // TODO: Put your codes here (lab5).
  TR::Level *out_most=TR::Outermost();
  //register FP() as the first temp;
  F::FP();
  //TR::Level *main=out_most->NewLevel(out_most, TEMP::NamedLabel("tigermain"),nullptr);
  ExpAndTy mainExpAndTy= root->Translate(E::BaseVEnv(),E::BaseTEnv(),out_most,nullptr);
  assert(mainExpAndTy.exp);
  TR::Func(mainExpAndTy.exp,out_most);
  //TR::procEntryExit(main,mainExpAndTy.exp,nullptr,true);
  return TR::getResult();
}

T::ExpList *ReverseArgs(TR::ExpList *t_explist)
{
  T::ExpList *res=nullptr,*temp;
  if(t_explist==nullptr) return res;
  TR::ExpList *pre=t_explist;
  while(pre)
  {
    //fprintf(stdout,"aaaaaaaaaaataaaa%daaaaaaaaaa\n",pre->head->UnEx()->kind);
    res=new T::ExpList(pre->head->UnEx(),res);
    pre=pre->tail;

  }
  return res;
}

}  // namespace TR
namespace {

   static TY::TyList *make_formal_tylist(S::Table<TY::Ty> *tenv, A::FieldList *params) {
  if (params == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(params->head->typ);
  assert(ty);
  return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
}

  
  static U::BoolList *make_escape_list(A::FieldList *fieldlist)
  {
    if(fieldlist)
    {
      return new U::BoolList(fieldlist->head->escape,make_escape_list(fieldlist->tail));
    }
    return nullptr;
  }

  static  TY::FieldList *make_fieldlist(S::Table<TY::Ty> *tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  assert(ty);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
}

}  // namespace
namespace A {

TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------simplevar-----------%s---\n",this->sym->Name().c_str());
  //fprintf(stderr,"simplevar : %s \n",this->sym->Name().c_str());
  E::EnvEntry *enventry=venv->Look(sym);
  TR::Exp *exp;
  assert(enventry);
  if(enventry!=nullptr)
  { 
    if(enventry->kind==E::EnvEntry::VAR)
    {
      fprintf(stderr,"%s   %d\n",this->sym->Name().c_str(),((E::VarEntry *)enventry)->ty->kind);
      E::VarEntry *varentry=(E::VarEntry*)enventry;
      assert(varentry->access);
      assert(varentry->access->access);
      assert(varentry->access->level);
      
      TR::Exp *ex=new TR::ExExp(F::Exp(varentry->access->access,
        level->staticLink(varentry->access->level)));

      return TR::ExpAndTy(ex,((E::VarEntry *)enventry)->ty);
    }
    assert(0);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  else{
    assert(0);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
}

TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------fieldvar--------------\n");
  TR::ExpAndTy varExpAndTy=var->Translate(venv,tenv,level,label);
  //assert(varExpAndTy.ty->ActualTy()->kind==TY::Ty::RECORD);
  
  if(varExpAndTy.ty->ActualTy()->kind!=TY::Ty::RECORD)
  {
    fprintf(stdout,"%d\n",varExpAndTy.ty->ActualTy()->kind);
    assert(0);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  TY::RecordTy *recordty=(TY::RecordTy *)(varExpAndTy.ty);
  TY::FieldList *recordfields=recordty->fields;
  TR::Exp *recordExp=(TR::ExExp *)varExpAndTy.exp;
  int index=0;
  assert(recordfields);
  fprintf(stderr,"     %s\n",recordfields->head->name->Name().c_str());
  for(recordfields=recordty->fields;recordfields;recordfields=recordfields->tail)
  {
    fprintf(stderr,"%s %s\n",recordfields->head->name->Name().c_str(),sym->Name().c_str());
    if(!recordfields->head->name->Name().compare(sym->Name()))
    {
      fprintf(stderr,"%s\n",sym->Name().c_str());
      T::Exp *addr=new T::BinopExp(T::PLUS_OP,recordExp->UnEx(),new T::ConstExp(index*F::wordSize));
      TR::Exp *memExp=new TR::ExExp(new T::MemExp(addr));
      return TR::ExpAndTy(memExp ,recordfields->head->ty->ActualTy());
    }
    index++;
  }
  assert(0);
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy SubscriptVar::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------subscriptvar--------------\n");
  TR::ExpAndTy varExpAndTy= var->Translate(venv,tenv,level,label);
  TY::Ty *varTy=varExpAndTy.ty;
  TR::Exp *varExp=varExpAndTy.exp;
  if(varTy->kind!=TY::Ty::ARRAY)
  {
    assert(0);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }

  TR::ExpAndTy subExpAndTy=subscript->Translate(venv,tenv,level,label);
  TY::Ty *subTy=subExpAndTy.ty;
  TR::Exp *subexp=(TR::ExExp*)subExpAndTy.exp;
  if(!subTy->IsSameType(TY::IntTy::Instance()))
  {
    assert(0);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }
  
  T::Exp *offset=new T::BinopExp(T::MUL_OP,subexp->UnEx(),new T::ConstExp(F::wordSize));
  T::Exp *addr=new T::BinopExp(T::PLUS_OP,varExp->UnEx(),offset);
  
  TR::Exp *memExp=new TR::ExExp(new T::MemExp(addr));
  return TR::ExpAndTy(memExp,((TY::ArrayTy *)varTy)->ty);
}

TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------varexp--------------\n");
  TR::ExpAndTy varExpAndTy=this->var->Translate(venv,tenv,level,label);
  varExpAndTy.ty=varExpAndTy.ty->ActualTy();
  return varExpAndTy;
}

TR::ExpAndTy NilExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------nilexp--------------\n");
  TR::Exp *exp=new TR::ExExp(new T::ConstExp(0));
  return TR::ExpAndTy(exp, TY::NilTy::Instance());
}

TR::ExpAndTy IntExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------intexp--------------\n");
  TR::Exp *exp=new TR::ExExp(new T::ConstExp(this->i));
  return TR::ExpAndTy(exp, TY::IntTy::Instance());
}

TR::ExpAndTy StringExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------stringexp--------------\n");
  TEMP::Label *newlabel=TEMP::NewLabel();
  F::Frag *frag=new F::StringFrag(newlabel,this->s);
  frags=new F::FragList(frag,frags);
  TR::Exp *exp=new TR::ExExp(new T::NameExp(newlabel));
  return TR::ExpAndTy(exp, TY::StringTy::Instance());
}

TR::ExpAndTy CallExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------callexp--------------\n");
  if (!venv->Look(func)){
   // assert(0);
    return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
  }

  E::EnvEntry *enventry = venv->Look(func);
  E::FunEntry *funcentry = (E::FunEntry *)enventry;
  TY::TyList *tylist = funcentry->formals;
  A::ExpList *a_explist=args;
  TY::Ty *resultTy;
  if(funcentry->result) {
    resultTy=funcentry->result;
  } else {
    resultTy=TY::VoidTy::Instance();
  }
  
  TR::ExpList *t_explist=nullptr;
  //T::ExpList head=T::ExpList(nullptr,nullptr),cur=head;
  for(a_explist;a_explist;a_explist=a_explist->tail)
  {
    TR::ExpAndTy argExpAndTy=a_explist->head->Translate(
      venv,tenv,level,label
    );
   
      t_explist=new TR::ExpList(argExpAndTy.exp,t_explist);
      argExpAndTy.exp->UnEx()->Print(stdout,0);
  }
  
  if(funcentry->level->parent)
  {
    T::Exp *linkExp= level->staticLink(funcentry->level->parent);
      T::Exp *callExp=new T::CallExp(new T::NameExp(funcentry->label),new T::ExpList(linkExp,TR::ReverseArgs(t_explist)));
    TR::Exp *exp=new TR::ExExp(callExp);
    return TR::ExpAndTy(exp,resultTy);
  }
   else{
     //fprintf(stdout,"------------no parent--------------\n");
     //fprintf(stdout,"%s\n",func->Name().c_str());
     //assert(TR::ReverseArgs(t_explist));
      TR::Exp *exp=new TR::ExExp(F::externalCall(TEMP::LabelString(func),TR::ReverseArgs(t_explist)));
      return TR::ExpAndTy(exp,resultTy);
   }   
  
}

TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------opexp--------------\n");
  TR::ExpAndTy leftExpAndTy=this->left->Translate(venv,tenv,level,label);
  TR::ExpAndTy rightExpAndTy=this->right->Translate(venv,tenv,level,label);

  T::Exp *t_exp=nullptr,*t_leftExp,*t_rightExp,*string_leftexp,*string_rightexp;
  T::Stm *t_stm=nullptr;
  TR::Exp *exp;
  t_leftExp=leftExpAndTy.exp->UnEx();
  t_rightExp=rightExpAndTy.exp->UnEx();
  string_leftexp=F::externalCall("stringEqual",
    new T::ExpList(leftExpAndTy.exp->UnEx(),
    new T::ExpList(rightExpAndTy.exp->UnEx(),nullptr)));
  string_rightexp=new T::ConstExp(1);

  if(leftExpAndTy.ty->kind==TY::Ty::STRING)
  {
    t_leftExp=string_leftexp;
    t_rightExp=string_rightexp;
  }
  switch (this->oper)
  {
  case A::Oper::PLUS_OP :
    t_exp=new T::BinopExp(T::PLUS_OP,t_leftExp,t_rightExp);
    break;
  case A::Oper::MINUS_OP:
    t_exp=new T::BinopExp(T::MINUS_OP,t_leftExp,t_rightExp);
    break;
  case A::Oper::DIVIDE_OP:
    t_exp=new T::BinopExp(T::DIV_OP,t_leftExp,t_rightExp);
    break;
  case A::Oper::TIMES_OP:
    t_exp=new T::BinopExp(T::MUL_OP,t_leftExp,t_rightExp);
    break;
  case A::Oper::EQ_OP:
    t_stm=new T::CjumpStm(T::EQ_OP,t_leftExp,t_rightExp,nullptr,nullptr);
    break;
  case A::Oper::NEQ_OP:
    t_stm=new T::CjumpStm(T::NE_OP,t_leftExp,t_rightExp,nullptr,nullptr);
    break;
  case A::Oper::LT_OP:
    t_stm=new T::CjumpStm(T::LT_OP,t_leftExp,t_rightExp,nullptr,nullptr);
    break;
  case A::Oper::LE_OP:
    t_stm=new T::CjumpStm(T::LE_OP,t_leftExp,t_rightExp,nullptr,nullptr);
    break;
  case A::Oper::GT_OP:
    t_stm=new T::CjumpStm(T::GT_OP,t_leftExp,t_rightExp,nullptr,nullptr);
    break;
  case A::Oper::GE_OP:
    t_stm=new T::CjumpStm(T::GE_OP,t_leftExp,t_rightExp,nullptr,nullptr);
    break;
  default:
    break;
  }
  if(t_exp!=nullptr)
      exp=new TR::ExExp(t_exp);
  else{
    if(t_stm!=nullptr)
    {
      TR::Cx *cx=new TR::Cx(new TR::PatchList( &((T::CjumpStm *)(t_stm))->true_label,nullptr),
        new TR::PatchList(&((T::CjumpStm *)(t_stm))->false_label,nullptr),
        t_stm);

      exp=new TR::CxExp(*cx);
    }
      
  }
  return TR::ExpAndTy(exp, TY::IntTy::Instance());
}

static T::Stm *createFields(TEMP::Temp *r,int length,TR::ExpList *fields)
{
  if(fields){
    return new T::SeqStm(
           new T::MoveStm(
           new T::MemExp(
             new T::BinopExp(T::PLUS_OP,new T::TempExp(r),new T::ConstExp(length*F::wordSize))
           ), fields->head->UnEx()
           ),
           createFields(r,length-1,fields->tail)
    );
  }else{
    return new T::ExpStm(new T::ConstExp(0));
  }
}
TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------recordexp--------------\n");
  TY::Ty *ty = tenv->Look(typ);
  ty=ty->ActualTy();
  T::Stm *stm;
  A::EFieldList *a_efieldlist;

  //
  T::Stm *seqStm1,*seqStm2;
  TR::ExpList *t_exps=nullptr;
  int length=0;
  for(a_efieldlist=this->fields;a_efieldlist; a_efieldlist=a_efieldlist->tail)
  {
      TR::ExpAndTy fieldExpAndTy=a_efieldlist->head->exp->Translate(
        venv,tenv,level,label
      );
      /*seqStm1=new T::SeqStm(new T::MoveStm(new T::BinopExp(
        T::PLUS_OP,new T::TempExp(temp),new T::ConstExp(F::wordSize)),
        fieldExpAndTy.exp->UnEx()),
        seqStm2);*/
      t_exps=new TR::ExpList(fieldExpAndTy.exp,t_exps);
      length++;
  }
  
  TEMP::Temp *temp=TEMP::Temp::NewTemp();
  stm=new T::MoveStm(new T::TempExp(temp),F::externalCall("allocRecord",
       new T::ExpList(new T::ConstExp(length *F::wordSize),nullptr)));
  T::Exp *recordExp=new T::EseqExp(stm,new T::EseqExp(
      createFields(temp,length-1,t_exps),new T::TempExp(temp)));
  TR::Exp *exp=new TR::ExExp(recordExp);
  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------seqexp--------------\n");
  A::ExpList *seqExp=this->seq;
  T::ExpList *t_explist=nullptr;
  TR::Exp *tr_exp=new TR::ExExp(new T::ConstExp(0));
  TY::Ty *ty=TY::VoidTy::Instance();
  if(seqExp==nullptr)
  {
    assert(0);
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }
  for(seqExp;seqExp;seqExp=seqExp->tail)
  {
    
    TR::ExpAndTy seqExpAndTy=seqExp->head->Translate(venv,tenv,level,label);
     //t_explist=new T::ExpList(seqExpAndTy.exp->UnEx(),t_explist);
    fprintf(stdout,"------------se--------xxxxxxxxxxxxxxxx------\n");
    
    tr_exp=new TR::ExExp(new T::EseqExp(tr_exp->UnNx(),seqExpAndTy.exp->UnEx()));
    //t_exp=new T::EseqExp(t,seqExpAndTy.exp->UnEx());
    ty=seqExpAndTy.ty;
   
  }
  fprintf(stdout,"------------seqexp--------------\n");
  return TR::ExpAndTy(tr_exp,ty);
}

TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------assigexp--------------\n");
  TR::ExpAndTy varExpAndTy=this->var->Translate(venv,tenv,level,label);
  this->var->Print(stderr,0);
  TR::ExpAndTy expExpAndTy=this->exp->Translate(venv,tenv,level,label);
  assert(varExpAndTy.exp);
  assert(expExpAndTy.exp);
  //assert(varExpAndTy.ty->IsSameType(expExpAndTy.ty));
  T::Stm *moveStm=new T::MoveStm(varExpAndTy.exp->UnEx(),expExpAndTy.exp->UnEx());
  TR::Exp *exp=new TR::NxExp(moveStm);
  fprintf(stdout,"------------assigexp--------------\n");
  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------ifexp--------------\n");
  TEMP::Label *t=TEMP::NewLabel();
  TEMP::Label *f=TEMP::NewLabel();
  TEMP::Label *done=TEMP::NewLabel();
  TEMP::Temp *temp=TEMP::Temp::NewTemp();
  TR::ExpAndTy testExpAndTy =this->test->Translate(venv,tenv,level,label);
  TR::ExpAndTy thenExpAndTY=this->then->Translate(venv,tenv,level,label);
  
  TR::Cx testCx= (testExpAndTy.exp->UnCx());
  TR::do_patch(testCx.trues,t);
  TR::do_patch(testCx.falses,f);
  TR::Exp *exp;
  //if then
  if(this->elsee==nullptr)
  {
    T::Stm *ifStm=new T::SeqStm(testCx.stm,new T::SeqStm(new T::LabelStm(t),
      new T::SeqStm(thenExpAndTY.exp->UnNx(),new T::LabelStm(f))));
    exp=new TR::NxExp(ifStm);
  }
  //if then else
  else{
    TR::ExpAndTy elseExpANdTy=this->elsee->Translate(venv,tenv,level,label);
    
    T::Exp *ifexp=new T::EseqExp(testCx.stm,
      new T::EseqExp(
        new T::LabelStm(t),
        new T::EseqExp(
          new T::MoveStm(new T::TempExp(temp),thenExpAndTY.exp->UnEx()),
          new T::EseqExp(
            new T::JumpStm(new T::NameExp(done),new TEMP::LabelList(done,nullptr)),
            new T::EseqExp(new T::LabelStm(f),
            new T::EseqExp(
            new T::MoveStm(new T::TempExp(temp),elseExpANdTy.exp->UnEx()),
            new T::EseqExp(new T::JumpStm(new T::NameExp(done),new TEMP::LabelList(done,nullptr)),
            new T::EseqExp(new T::LabelStm(done),new T::TempExp(temp)
          )
        )
      ))
          ))));
      exp=new TR::ExExp(ifexp);
  }
  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------whileexp--------------\n");
  
  TEMP::Label *t=TEMP::NewLabel();
  TEMP::Label *l=TEMP::NewLabel();
  TEMP::Label *done=TEMP::NewLabel();
  this->test->Print(stdout,0);
  //this->body->Print(stdout,0);
  TR::ExpAndTy testExpAndTy =this->test->Translate(venv,tenv,level,label);
  TR::ExpAndTy bodyExpAndTY=this->body->Translate(venv,tenv,level,done);
  fprintf(stdout,"***********************\n");
  this->test->Print(stdout,0);
  fprintf(stdout,"***********************\n");
  //this->body->Print(stdout,0);
  TR::Cx testCx= (testExpAndTy.exp->UnCx());
  TR::do_patch(testCx.trues,l);
  TR::do_patch(testCx.falses,done);

  T::Stm *stmNx=new T::SeqStm(new T::LabelStm(t),new T::SeqStm(testCx.stm,
  new T::SeqStm(new T::LabelStm(l),new T::SeqStm(bodyExpAndTY.exp->UnNx(),
  new T::SeqStm(new T::JumpStm(new T::NameExp(t),new TEMP::LabelList(t,nullptr)),
  new T::LabelStm(done))) )));

  TR::Exp *exp=new TR::NxExp(stmNx);
  fprintf(stdout,"------------whileexp--------------\n");
  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------forexp--------------\n");
  venv->BeginScope();
  tenv->BeginScope();
  // i enter
  E::VarEntry varentry(TY::IntTy::Instance(),true);
  TR::ExpAndTy lowExpAndTy =this->lo->Translate(venv,tenv,level,label);
  TR::ExpAndTy hiExpAndTy= this->hi->Translate(venv,tenv,level,label);
  

  TEMP::Label *done= TEMP::NewLabel();
  TR::Access *loopAccess=level->AllocLocal(true);
  fprintf(stderr,"for i enter\n");
  venv->Enter(var,new E::VarEntry(loopAccess, TY::IntTy::Instance(),true));

  TR::ExpAndTy bodyExpAndTy =this->body->Translate(venv,tenv,level,done);

  tenv->EndScope();
  venv->EndScope();

  TEMP::Label *lo=TEMP::NewLabel();
  TEMP::Label *body=TEMP::NewLabel();
  TEMP::Temp *limit=TEMP::Temp::NewTemp();

  T::Exp *fp=level->staticLink(loopAccess->level);
  T::Exp *f_exp=F::Exp(loopAccess->access,fp);
  TR::Exp *loop_i_exp=new TR::ExExp(f_exp);

  //low<=high before loop
  //i<=limit
  T::Stm *stmNx=new T::SeqStm(
    new T::MoveStm(loop_i_exp->UnEx(),lowExpAndTy.exp->UnEx()),
    new T::SeqStm(new T::MoveStm(new T::TempExp(limit),hiExpAndTy.exp->UnEx()),
    new T::SeqStm(new T::CjumpStm(T::GE_OP,loop_i_exp->UnEx(),new T::TempExp(limit),done,body),
    new T::SeqStm(new T::LabelStm(body),new T::SeqStm(bodyExpAndTy.exp->UnNx(),
    new T::SeqStm(new T::CjumpStm(T::EQ_OP,loop_i_exp->UnEx(),new T::TempExp(limit),done,lo),
    new T::SeqStm(new T::LabelStm(lo),
    new T::SeqStm(new T::MoveStm(loop_i_exp->UnEx(),
    new T::BinopExp(T::PLUS_OP,loop_i_exp->UnEx(),new T::ConstExp(1))),
    new T::SeqStm(new T::JumpStm(new T::NameExp(body),new TEMP::LabelList(body,nullptr)),
    new T::LabelStm(done))))))))));
  
  TR::Exp *exp=new TR::NxExp(stmNx);
  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------breakexp--------------\n");
  if(label)
  {
  T::Stm *stmNx=new T::JumpStm(new T::NameExp(label),new TEMP::LabelList(label,nullptr));
  TR::Exp *exp=new TR::NxExp(stmNx);
  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
  }
  else{
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)),TY::VoidTy::Instance());
  }
}

TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  //fprintf(stderr,"letexp tr\n");
  fprintf(stdout,"------------letexp--------------\n");
  DecList *d;
  venv->BeginScope();
  tenv->BeginScope();
  TR::Exp *tr_exp=new TR::ExExp(new T::ConstExp(0));
  T::Stm *t_stm=nullptr;
  for(d=decs;d;d=d->tail)
  {
    fprintf(stderr,"letexp trd\n");
    tr_exp= new TR::ExExp(new T::EseqExp(tr_exp->UnNx(),d->head->Translate(venv,tenv,level,label)->UnEx())); 
  }
  TR::ExpAndTy bodyExpAndTy=this->body->Translate(venv,tenv,level,label);
  bodyExpAndTy.exp=new TR::ExExp(new T::EseqExp(tr_exp->UnNx(),bodyExpAndTy.exp->UnEx()));
  tenv->EndScope();
  venv->EndScope();
  return bodyExpAndTy;
}

TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------arrayexp--------------\n");
  TY::Ty *ty=tenv->Look(typ);
  assert(ty);
  TY::ArrayTy *arrayTy;
  ty=ty->ActualTy();
 // arrayTy=(TY::ArrayTy*)ty;
  std::string func="initArray";
  TR::ExpAndTy initExpAndTy=this->init->Translate(venv,tenv,level,label);
  TR::ExpAndTy sizeExpAndTy=this->size->Translate(venv,tenv,level,label);
  TEMP::Temp *res=TEMP::Temp::NewTemp();
  TEMP::Temp *sizeTemp=TEMP::Temp::NewTemp(),*initTemp=TEMP::Temp::NewTemp();
  T::Exp *arrayExp=new T::EseqExp(new T::MoveStm(new T::TempExp(sizeTemp),sizeExpAndTy.exp->UnEx()),
                    new T::EseqExp(new T::MoveStm(new T::TempExp(initTemp),initExpAndTy.exp->UnEx()),
                    new T::EseqExp(new T::MoveStm(new T::TempExp(res),
                    F::externalCall("initArray",new T::ExpList(new T::TempExp(sizeTemp),
                    new T::ExpList(new T::TempExp(initTemp),nullptr)))),
                    new T::TempExp(res))));

  TR::Exp *exp=new TR::ExExp(arrayExp);
  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------voidexp--------------\n");
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
}

TR::Exp *FunctionDec::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------functiondec--------------\n");
  TY::Ty *result_ty;
  FunDecList *funList;
  A::FunDec *funDec;
  A::FieldList *fieldlist;
  E::FunEntry *funcentry;
  TY::TyList *tylist;

  //first iterator, function name,and enter venv
  for (funList=functions;funList;funList=funList->tail)
  {
    int count=0;
    funDec = funList->head;
    result_ty = TY::VoidTy::Instance();
    if (funDec->result&&(funDec->result->Name().compare("")!=0)) {
      result_ty = tenv->Look(funDec->result);
    }
    
    //
    tylist = make_formal_tylist(tenv, funDec->params);
  
    TY::TyList *formalTys=make_formal_tylist(tenv,funDec->params);
    TEMP::Label *new_label=TEMP::NewLabel();
    U::BoolList *escape_list=make_escape_list(funDec->params);
    TR::Level *new_level=level->NewLevel(level,new_label,escape_list);
    venv->Enter(funDec->name, new E::FunEntry(new_level,new_label,tylist, result_ty));
  
  }

  //second iterator, check body
  for(funList=functions;funList; funList=funList->tail)
  {
    venv->BeginScope();
    funDec = funList->head;
    fieldlist = funDec->params;
    fprintf(stderr,"func name:  %s\n",funDec->name->Name().c_str());
    funcentry = (E::FunEntry *)venv->Look(funDec->name);
    tylist = funcentry->formals;
    TR::AccessList *accesslist=funcentry->level->Formals();
    //assert(accesslist);
    while (tylist && fieldlist)
    {
      fprintf(stderr,"param: %s\n",fieldlist->head->name->Name().c_str());
      venv->Enter(fieldlist->head->name, new E::VarEntry(accesslist->head, tylist->head));
      tylist = tylist->tail;
      fieldlist= fieldlist->tail;
      accesslist=accesslist->tail;
    }
    TR::ExpAndTy bodyExpAndTy=funDec->body->Translate(venv,tenv,funcentry->level,label);
    result_ty=bodyExpAndTy.ty;
    //TR::procEntryExit(funcentry->level,bodyExpAndTy.exp,accesslist);
    venv->EndScope();
    TR::Func(bodyExpAndTy.exp,funcentry->level);
  }
  return new TR::ExExp(new T::ConstExp(0));
}

TR::Exp *VarDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                           TR::Level *level, TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------vardec--------------\n");
  TR::ExpAndTy decExpAndTy=this->init->Translate(venv,tenv,level,label);
  TR::Access *access=level->AllocLocal(true);
  if(this->typ)
  {
    TY::Ty *existty=tenv->Look(this->typ);
    existty=existty->ActualTy();
    venv->Enter(var,new E::VarEntry(access,existty,true));
  }
  else{
  venv->Enter(var,new E::VarEntry(access, decExpAndTy.ty,true));
  }

  T::Stm *varStm=new T::MoveStm(
    (new TR::ExExp(F::Exp(access->access,level->staticLink(access->level))))->UnEx(),
    decExpAndTy.exp->UnEx());
  
  TR::Exp *exp=new TR::NxExp(varStm);
  return exp;
}

TR::Exp *TypeDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                            TR::Level *level, TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------typedec--------------\n");
  A::NameAndTyList *sem_types=types,*tyList;
  A::NameAndTy *nameTy;
  TY::Ty *idtype;
  for(sem_types; sem_types;sem_types=sem_types->tail)
  {
    tenv->Enter(sem_types->head->name,new TY::NameTy(sem_types->head->name,nullptr));
  }

  tyList = types;
  while(tyList) {
    TY::Ty *ty = tenv->Look(tyList->head->name);
    nameTy=tyList->head;
    switch (nameTy->ty->kind)
    {
    case A::Ty::NAME:
      ((TY::NameTy*)ty)->sym=((A::NameTy*)nameTy->ty)->name;
      
      break;
    case A::Ty::RECORD:
      ty=new TY::RecordTy(make_fieldlist(tenv,((A::RecordTy*)nameTy->ty)->record));
      break;
    default:
      ty=new TY::ArrayTy(tenv->Look(((A::ArrayTy*)nameTy->ty)->array));
      break;
    }
    //assert(ty->kind==TY::Ty::NAME);
    //((TY::NameTy *)ty)->ty = tyList->head->ty->Translate(tenv);
    tyList = tyList->tail;
  }
  
  /*//check record type and array type
  tyList=types;
  while(tyList)
  {
    nameTy=tyList->head;
    switch (nameTy->ty->kind)
    {
    case A::Ty::NAME:
      break;
    case A::Ty::RECORD:
    {
      idtype=tenv->Look(nameTy->name);
      TY::RecordTy *recordTy=(TY::RecordTy *) idtype;
      recordTy->fields=
      break;
    }
    
    default:
      break;
    }
  }*/
  //check recursive
  /*
  tyList=types;
  while(tyList) {
    TY::Ty *ty = tenv->Look(tyList->head->name);
    if(ty->kind == TY::Ty::NAME) {
      TY::Ty *tyTy = ((TY::NameTy *)ty)->ty;
      while (tyTy->kind == TY::Ty::NAME)
      {
        TY::NameTy *nameTy = (TY::NameTy *)tyTy;
        if(!nameTy->sym->Name().compare(tyList->head->name->Name())){
          return new TR::ExExp(new T::ConstExp(0));
        }
        tyTy = nameTy->ty;
      }
    }
    tyList = tyList->tail;
  }*/
  return new TR::ExExp(new T::ConstExp(0));
}

TY::Ty *NameTy::Translate(S::Table<TY::Ty> *tenv) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------namety--------------\n");
  TY::Ty *type=tenv->Look(this->name);
  while(type&&type->kind==TY::Ty::NAME)
  {
    TY::NameTy *nameTy=(TY::NameTy *)type;
    if(nameTy->ty)
    {
      type=nameTy->ty;
    }
    else{
      break;
    }
  }
  return type;
}

TY::Ty *RecordTy::Translate(S::Table<TY::Ty> *tenv) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------recordty--------------\n");
  TY::FieldList *fieldlist=make_fieldlist(tenv,record);
  
  fprintf(stderr,"        %s\n",fieldlist->head->name->Name().c_str());
  return new TY::RecordTy(fieldlist);
}

TY::Ty *ArrayTy::Translate(S::Table<TY::Ty> *tenv) const {
  // TODO: Put your codes here (lab5).
  fprintf(stdout,"------------arrayty--------------\n");
  TY::Ty *ty=tenv->Look(array);
  return new TY::ArrayTy(ty);
}

}  // namespace A
