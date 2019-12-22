#include "tiger/escape/escape.h"

namespace ESC {
class EscapeEntry {
 public:
  int depth;
  bool* escape;

  EscapeEntry(int depth, bool* escape) : depth(depth), escape(escape) {}
};

static void traverseExp(S::Table<ESC::EscapeEntry> *venv,int depth,A::Exp *exp);
static void traverseDec(S::Table<ESC::EscapeEntry> *venv,int depth,A::Dec *dec);
static void traverseVar(S::Table<ESC::EscapeEntry> *venv,int depth,A::Var *var);

static void traverseExp(S::Table<ESC::EscapeEntry> *venv,int depth,A::Exp *exp)
{
  switch (exp->kind)
  {
  case A::Exp::VAR:
  {
    /* code */
    A::VarExp *varexp=(A::VarExp *)exp;
    traverseVar(venv,depth,varexp->var);
    break;
  }
  case A::Exp::NIL:
    break;
  case A::Exp::INT:
    break;
  case A::Exp::STRING:
    break;
  case A::Exp::CALL:
  {
    A::CallExp *callexp=(A::CallExp *) exp;
    A::ExpList *args=callexp->args;
    for(args;args;args=args->tail)
    {
      traverseExp(venv,depth,args->head);
    }
    break;
  }
  case A::Exp::OP:
  {
    A::OpExp *opexp=(A::OpExp *)exp;
    traverseExp(venv,depth,opexp->left);
    traverseExp(venv,depth,opexp->right);
    break;
  }
  case A::Exp::RECORD:
  {
    A::RecordExp *recordexp=(A::RecordExp *)exp;
    A::EFieldList *efields=recordexp->fields;
    for(efields;efields;efields=efields->tail)
    {
      traverseExp(venv,depth,efields->head->exp);
    }
    break;
  }

  case A::Exp::SEQ:
  {
    A::SeqExp *seqexp=(A::SeqExp*) exp;
    A::ExpList *explist=seqexp->seq;
    for(explist;explist;explist=explist->tail)
    {
      traverseExp(venv,depth,explist->head);
    }
    break;
  }

  case A::Exp::ASSIGN:
  {
    A::AssignExp *assignexp=(A::AssignExp*)exp;
    A::Var *var=assignexp->var;
    A::Exp *exp=assignexp->exp;
    traverseExp(venv,depth,exp);
    traverseVar(venv,depth,var);
    break;
  }

  case A::Exp::IF:
  {
    A::IfExp *ifexp=(A::IfExp *)exp;
    traverseExp(venv,depth,ifexp->test);
    traverseExp(venv,depth,ifexp->then);
    if(ifexp->elsee)
    {
      traverseExp(venv,depth,ifexp->elsee);
    }
    break;
  }

  case A::Exp::WHILE:
  {
    A::WhileExp *whileexp=(A::WhileExp *)exp;
    A::Exp *test=whileexp->test;
    A::Exp *body=whileexp->body;
    traverseExp(venv,depth,test);
    traverseExp(venv,depth,body);
    break;
  }

  case A::Exp::FOR:
  {
    A::ForExp *forexp=(A::ForExp *) exp;
    A::Exp *lo=forexp->lo;
    A::Exp *hi=forexp->hi;
    A::Exp *body=forexp->body;
    traverseExp(venv,depth,lo);
    traverseExp(venv,depth,hi);
    venv->BeginScope();
    forexp->escape=false;
    venv->Enter(forexp->var,new ESC::EscapeEntry(depth,&forexp->escape));
    traverseExp(venv,depth,body);
    venv->EndScope();
    break;
  }

  case A::Exp::BREAK:
  {
    break;
  }

  case A::Exp::LET:{
    A::LetExp *letexp=(A::LetExp*) exp;
     A::DecList *decs=letexp->decs;
    venv->BeginScope();
    for(decs;decs;decs=decs->tail)
    {
      traverseDec(venv,depth,decs->head);
    }
    traverseExp(venv,depth,letexp->body);
    venv->EndScope();
    break;
  }

  case A::Exp::ARRAY:
  {
    A::ArrayExp *arrayexp=(A::ArrayExp *) exp;
    traverseExp(venv,depth,arrayexp->size);
    traverseExp(venv,depth,arrayexp->init);
    break;
  }
  default:
    break;
  }
}
static void traverseDec(S::Table<ESC::EscapeEntry> *venv,int depth,A::Dec *dec)
{
  switch (dec->kind)
  {
  case A::Dec::FUNCTION:
    {
      A::FunctionDec *funcDec=(A::FunctionDec *)dec;
      A::FunDecList *functions=funcDec->functions;
      A::FunDec *func=nullptr;
      A::FieldList *params;
      for(functions;functions;functions=functions->tail)
      {
        func=functions->head;
        venv->BeginScope();
        for(params=func->params;params;params=params->tail)
        {
          params->head->escape=false;
          venv->Enter(params->head->name,new ESC::EscapeEntry(depth+1,&params->head->escape));
        }
        traverseExp(venv,depth+1,func->body);
        venv->EndScope();
      }
    }
    break;
  case A::Dec::VAR:
  {
    A::VarDec *vardec=(A::VarDec *)dec;
    S::Symbol *var=vardec->var;
    A::Exp *init=vardec->init;
    traverseExp(venv,depth,init);
    vardec->escape=false;
    venv->Enter(var,new ESC::EscapeEntry(depth,&vardec->escape));
    break;
  }
  default:
    break;
  }
}
static void traverseVar(S::Table<ESC::EscapeEntry> *venv,int depth,A::Var *var)
{
  switch (var->kind)
  {
  case A::Var::SIMPLE:
  {
    A::SimpleVar *simplevar=(A::SimpleVar*)var;
    S::Symbol *id=simplevar->sym;
    EscapeEntry *entry=venv->Look(id);
    if(entry->depth<depth){
      *(entry->escape)=true;
      venv->Set(id,entry);
    }
    break;
  }
  case A::Var::FIELD:
  {
    A::FieldVar *fieldvar=(A::FieldVar *)var;
    traverseVar(venv,depth,fieldvar->var);
    break;
  }
  
  case A::Var::SUBSCRIPT:
  {
    A::SubscriptVar *subscriptvar=(A::SubscriptVar *)var;
    traverseVar(venv,depth,subscriptvar->var);
    traverseExp(venv,depth,subscriptvar->subscript);
    break;
  }
  default:
    break;
  }
}


void FindEscape(A::Exp* exp) {
  // TODO: Put your codes here (lab6).
  ESC::traverseExp(new S::Table<EscapeEntry>(),0,exp);  
}



}  // namespace ESC