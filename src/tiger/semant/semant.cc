#include "tiger/semant/semant.h"
#include "tiger/errormsg/errormsg.h"
#include<iostream>
using namespace std;
extern EM::ErrorMsg errormsg;

using VEnvType = S::Table<E::EnvEntry> *;
using TEnvType = S::Table<TY::Ty> *;

namespace {
static TY::TyList *make_formal_tylist(TEnvType tenv, A::FieldList *params) {
  if (params == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(params->head->typ);
  if (ty == nullptr) {
    errormsg.Error(params->head->pos, "undefined type %s",
                   params->head->typ->Name().c_str());
  }

  return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
}

static TY::FieldList *make_fieldlist(TEnvType tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  if (ty == nullptr) {  errormsg.Error(fields->head->pos, "undefined type %s",
       fields->head->typ->Name().c_str());
  }
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
}

}  // namespace


namespace A {

TY::Ty *SimpleVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  // TODO: Put your codes here (lab4).
  E::EnvEntry *enventry=venv->Look(sym);
  if(enventry!=nullptr)
  { 
    return ((E::VarEntry *)enventry)->ty;
  }
  else{
    errormsg.Error(pos,"undefined variable %s",sym->Name().c_str());
    return TY::VoidTy::Instance();
  }
}

TY::Ty *FieldVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *varty;
  varty=var->SemAnalyze(venv,tenv,labelcount)->ActualTy();
  if(varty->kind!=TY::Ty::RECORD)
  {
    errormsg.Error(pos,"not a record type");
    return TY::VoidTy::Instance();
  }
  TY::RecordTy *recordty=(TY::RecordTy *)varty;
  TY::FieldList *recordfields=recordty->fields;
  for(recordfields,recordfields;recordfields=recordfields->tail;)
  {
    if(!recordfields->head->name->Name().compare(sym->Name()))
    {
      return recordfields->head->ty;
    }
  }
  errormsg.Error(pos,"field %s doesn't exist",sym->Name().c_str());
  return TY::VoidTy::Instance();
}

TY::Ty *SubscriptVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *varty=var->SemAnalyze(venv,tenv,labelcount);
  if(varty->kind!=TY::Ty::ARRAY)
  {
    errormsg.Error(pos,"array type required");
  }
  TY::Ty *subty=subscript->SemAnalyze(venv,tenv,labelcount);
  if(!subty->IsSameType(TY::IntTy::Instance()))
  {
    errormsg.Error(pos,"array variable's subscript must be integer");
  }
  return varty->ActualTy();
}

TY::Ty *VarExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  return var->SemAnalyze(venv, tenv, labelcount)->ActualTy();
}

TY::Ty *NilExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  return TY::NilTy::Instance();
}

TY::Ty *IntExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  return TY::IntTy::Instance();
}

TY::Ty *StringExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  // TODO: Put your codes here (lab4).
  return TY::StringTy::Instance();
}

TY::Ty *CallExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  // TODO: Put your codes here (lab4).

  if (!venv->Look(func)){
    errormsg.Error(pos, "undefined function %s", func->Name().c_str());
    return TY::VoidTy::Instance();
  }

  E::EnvEntry *enventry = venv->Look(func);
  E::FunEntry *funcentry = (E::FunEntry *)enventry;
  TY::TyList *tylist = funcentry->formals;
  A::ExpList *explist = args;

  while(tylist && explist) {
    TY::Ty *ty = explist->head->SemAnalyze(venv, tenv, labelcount);
    if(!ty->IsSameType(tylist->head)){
      errormsg.Error(pos, "para type mismatch");
    }
    tylist = tylist->tail;
    explist = explist->tail;
  } 

  if(tylist) {
    errormsg.Error(pos, "missing params in function %s", func->Name().c_str());
  }
  if(explist) {
    errormsg.Error(pos, "too many params in function %s", func->Name().c_str());
  }
  if(funcentry->result) {
    return funcentry->result;
  } else return TY::VoidTy::Instance();
}


TY::Ty *OpExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *leftTy = left->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *rightTy = right->SemAnalyze(venv, tenv, labelcount);

      
  switch (oper)
  {
  case PLUS_OP:case MINUS_OP:case TIMES_OP:case DIVIDE_OP:
    if (!leftTy->IsSameType(TY::IntTy::Instance()) || !rightTy->IsSameType(TY::IntTy::Instance()))
      errormsg.Error(pos, "integer required");
    break;
  default:
    if (!leftTy->IsSameType(rightTy))
      errormsg.Error(pos, "same type required");
    break;
  }
  return TY::IntTy::Instance();
}


TY::Ty *RecordExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
    // TODO: Put your codes here (lab4).
  TY::Ty *ty = tenv->Look(typ);
  if (!ty) {
    errormsg.Error(pos, "undefined type %s", typ->Name().c_str());
    return TY::VoidTy::Instance();
  }
  return ty;
}

TY::Ty *SeqExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  A::ExpList *seqexp;
  TY::Ty *explist_ty;
  for(seqexp=seq;seqexp;seqexp=seqexp->tail)
  {
    explist_ty= seqexp->head->SemAnalyze(venv,tenv,labelcount);
  }
  return explist_ty;
}

TY::Ty *AssignExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  // TODO: Put your codes here (lab4).
  if(var->kind==A::Var::SIMPLE)
  {
    A::SimpleVar *simplevar=(A::SimpleVar *) var;
    E::EnvEntry *enventry=venv->Look(simplevar->sym);
   if(enventry->readonly)
    {
      errormsg.Error(pos,"loop variable can't be assigned");
    }
  }

  TY::Ty *varty=var->SemAnalyze(venv,tenv,labelcount);
  TY::Ty *expty=exp->SemAnalyze(venv,tenv,labelcount);
  if(!varty->IsSameType(expty))
  {
    errormsg.Error(pos,"unmatched assign exp");
  }
  return TY::VoidTy::Instance();
}

TY::Ty *IfExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *test_ty,*then_ty,*elsee_ty;
  test_ty=test->SemAnalyze(venv,tenv,labelcount);
  then_ty=then->SemAnalyze(venv,tenv,labelcount);
  if(!test_ty||!test_ty->IsSameType(TY::IntTy::Instance())){
     errormsg.Error(pos, "if test must be integer");
  }

  if(elsee!=nullptr)
  {
    elsee_ty=elsee->SemAnalyze(venv,tenv,labelcount);
    if(!then_ty->IsSameType(elsee_ty)){
      errormsg.Error(pos, "then exp and else exp type mismatch");
    }
    return then_ty;
  }
  else
    if(!then_ty||!then_ty->IsSameType(TY::VoidTy::Instance()))
    {
      errormsg.Error(pos,"if-then exp's body must produce no value");
    }
  return TY::VoidTy::Instance();
}

TY::Ty *WhileExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *test_ty=test->SemAnalyze(venv,tenv,labelcount+1);
  TY::Ty *bodyty=body->SemAnalyze(venv,tenv,labelcount+1);
  if(!test_ty||!test_ty->IsSameType(TY::IntTy::Instance())){
    errormsg.Error(pos,"while test must be integer");
  }
  if(bodyty->kind!=TY::Ty::VOID)
  {
    errormsg.Error(pos,"while body must produce no value");
  }
  return TY::VoidTy::Instance();
}

TY::Ty *ForExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *lo_type,*hi_type,*body_type,*var_type;

  venv->BeginScope();
  E::VarEntry varentry(TY::IntTy::Instance(),true);
  venv->Enter(var,&varentry);
  lo_type=lo->SemAnalyze(venv,tenv,labelcount);
  hi_type=hi->SemAnalyze(venv,tenv,labelcount);
  
  if(lo_type->kind!=TY::Ty::INT||hi_type->kind!=TY::Ty::INT)
  {
    errormsg.Error(pos,"for exp's range type is not integer");
  }
  body_type=body->SemAnalyze(venv,tenv,labelcount);
  if(!body_type->IsSameType(TY::VoidTy::Instance()))
  {
    errormsg.Error(pos,"for expression must produce no value");
  }
  venv->EndScope();
  return TY::VoidTy::Instance();
}

TY::Ty *BreakExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // TODO: Put your codes here (lab4).
  return TY::VoidTy::Instance();
}


TY::Ty *LetExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  DecList *d;
  TY::Ty *ty;
  venv->BeginScope();
  tenv->BeginScope();

  for(d=decs;d;d=d->tail)
  {
     d->head->SemAnalyze(venv,tenv,labelcount);
  }
  ty=body->SemAnalyze(venv,tenv,labelcount);

  tenv->EndScope();
  venv->EndScope();
  return ty;
}

TY::Ty *ArrayExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *ty,*int_ty;
  TY::ArrayTy *array_ty;
  
  ty = tenv->Look(typ);
  if(!ty) {
    errormsg.Error(pos, "undefiend type %s", typ->Name().c_str());
  }
  ty=ty->ActualTy();
  if(ty->ActualTy()->kind != TY::Ty::ARRAY) {
    errormsg.Error(pos, "not array type");
  }

  int_ty = size->SemAnalyze(venv, tenv, labelcount);
  if (!int_ty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(pos, "integer required");
  }
  
  array_ty=(TY::ArrayTy *)ty;
  ty = init->SemAnalyze(venv, tenv, labelcount);
  if(!ty->IsSameType(array_ty->ty)){
    errormsg.Error(pos, "type mismatch");
  }
  return array_ty;
}

TY::Ty *VoidExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  // TODO: Put your codes here (lab4).
  return TY::VoidTy::Instance();
}
void FunctionDec::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // TODO: Put your codes here (lab4).
  TY::Ty *result_ty;
  A::FunDecList *funList;
  A::FunDec *funDec;
  A::FieldList *fieldlist;
  E::FunEntry *funcentry;
  TY::TyList *tylist;

  //first iterator, function name,and enter venv
  for (funList=functions;funList;funList=funList->tail)
  {
    funDec = funList->head;
    result_ty = TY::VoidTy::Instance();
    if (funDec->result) {
      result_ty = tenv->Look(funDec->result);
    }
    
    //
    tylist = make_formal_tylist(tenv, funDec->params);
    if (venv->Look(funDec->name)) {
      errormsg.Error(pos, "two functions have the same name");
    }
    else venv->Enter(funDec->name, new E::FunEntry(tylist, result_ty));
  }

  //second iterator, check body
  for(funList=functions;funList; funList=funList->tail)
  {
    venv->BeginScope();
    funDec = funList->head;
    fieldlist = funDec->params;
    funcentry = (E::FunEntry *)venv->Look(funDec->name);
    tylist = funcentry->formals;
    while (tylist && fieldlist)
    {
      venv->Enter(fieldlist->head->name, new E::VarEntry(tylist->head));
      tylist = tylist->tail;
      fieldlist= fieldlist->tail;
    }
    result_ty = funDec->body->SemAnalyze(venv, tenv, labelcount);

    if(!result_ty->IsSameType(funcentry->result)) {
      if(funcentry->result->IsSameType(TY::VoidTy::Instance())) {
        errormsg.Error(pos, "procedure returns value");
      } else {
        errormsg.Error(pos, "function return type mismatch");
      }
    }
    venv->EndScope();
  }
  
}

void VarDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).

  //correctness of init
  TY::Ty *init_ty=init->SemAnalyze(venv,tenv,labelcount);
  if(venv->Look(var))
  {
    errormsg.Error(pos,"two variable have the same name");
    return;
  }
  if(init_ty==nullptr)
  {
    errormsg.Error(pos,"invalid init");
  }
  
  // the correctness of typ
  if(typ!=nullptr){
    TY::Ty *typ_ty=tenv->Look(typ);
    if(typ_ty==nullptr)
    {
      errormsg.Error(pos,"undefined type of %s", typ->Name().c_str());
    }else{
        if(!typ_ty->IsSameType(init_ty))
        {
          errormsg.Error(pos,"type mismatch");
        }
    }
    venv->Enter(var,new E::VarEntry(init_ty));
  }else{
    if(init_ty->IsSameType(TY::NilTy::Instance()) && init_ty->ActualTy()->kind != TY::Ty::RECORD) {
      errormsg.Error(pos, "init should not be nil without type specified");
    } else {
      venv->Enter(var, new E::VarEntry(init_ty));
    }
  }

}

void TypeDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  // TODO: Put your codes here (lab4).
  NameAndTyList *sem_types,*tyList;
  sem_types=types;
  TY::NameTy *tmp_ty;
  //to handle recursive dec, use two transverse
  for(sem_types;sem_types;sem_types=sem_types->tail)
  {
    if(tenv->Look(sem_types->head->name))
    {
      errormsg.Error(pos,"two types have the same name");
    }
    tmp_ty=new TY::NameTy(sem_types->head->name,nullptr);
    tenv->Enter(sem_types->head->name,tmp_ty);
  }

  tyList = types;
  while(tyList) {
    TY::Ty *ty = tenv->Look(tyList->head->name);
    ((TY::NameTy *)ty)->ty = tyList->head->ty->SemAnalyze(tenv);
    tyList = tyList->tail;
  }

  //check recursive
  tyList=types;
  while(tyList) {
    TY::Ty *ty = tenv->Look(tyList->head->name);
    if(ty->kind == TY::Ty::NAME) {
      TY::Ty *tyTy = ((TY::NameTy *)ty)->ty;
      while (tyTy->kind == TY::Ty::NAME)
      {
        TY::NameTy *nameTy = (TY::NameTy *)tyTy;
        if(!nameTy->sym->Name().compare(tyList->head->name->Name())){
          errormsg.Error(pos, "illegal type cycle");
          return;
        }
        tyTy = nameTy->ty;
      }
    }
    tyList = tyList->tail;
  }
}

TY::Ty *NameTy::SemAnalyze(TEnvType tenv) const {
  // TODO: Put your codes here (lab4).
  if(!tenv->Look(name)){
    errormsg.Error(pos,"undefined type %s",name->Name().c_str());
  }
  return tenv->Look(name);
}

TY::Ty *RecordTy::SemAnalyze(TEnvType tenv) const {
  // TODO: Put your codes here (lab4).
  TY::FieldList *fieldlist=make_fieldlist(tenv,record);
  return new TY::RecordTy(fieldlist);
}

TY::Ty *ArrayTy::SemAnalyze(TEnvType tenv) const {
  // TODO: Put your codes here (lab4).
  
  TY::Ty *ty=tenv->Look(array);
  if(ty==nullptr)
  {
    errormsg.Error(pos,"undefined type %s",array->Name().c_str());
  }
  return new TY::ArrayTy(ty);
}
  // namespace A
}
namespace SEM {
void SemAnalyze(A::Exp *root) {
  if (root) root->SemAnalyze(E::BaseVEnv(), E::BaseTEnv(), 0);
}

}  // namespace SEM