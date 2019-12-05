#include "tiger/regalloc/regalloc.h"
#include<cstdio>
#include<sstream>
namespace RA {

static AS::InstrList *iList=nullptr,*iLast=nullptr;

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


AS::InstrList *rewriteInstrList(F::Frame* frame,AS::InstrList *il,TEMP::Map *map);
Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  // TODO: Put your codes here (lab6).
  Result res;
  TEMP::Map *map=TEMP::Map::Name();
  map->Enter(F::RSP(),new std::string("%rbp"));
  map->Enter(F::RBP(),new std::string("%rbp"));
  //MOVE REGISTER
  map->Enter(F::RCX(),new std::string("%rcx"));
  //ALU SRC REGISTER
  map->Enter(F::R8(),new std::string("%r8"));
  map->Enter(F::RDX(),new std::string("%rdx"));
  //ALU DST REGISTER
  map->Enter(F::RAX(),new std::string("%rax"));

  f->size++;
  fprintf(stdout,"------------------frame size-----------%d\n",f->size*F::wordSize);
  AS::InstrList *instrlist=rewriteInstrList(f,il,map);
  res.il=iList;
  res.coloring=map;
  return res;
}


AS::InstrList *rewriteInstrList(F::Frame* frame,AS::InstrList *il,TEMP::Map *map)
{
  AS::InstrList *instrlist;
  AS::InstrList *result;
  int maxsize=frame->size;
  
  //alloc position in the frame
  for(instrlist=il;instrlist; instrlist=instrlist->tail)
  {
    std::string assm;
    switch (instrlist->head->kind)
    {
    case AS::Instr::OPER:
    {
      AS::OperInstr *operinstr=(AS::OperInstr *)instrlist->head;
      if(operinstr->src==nullptr&&operinstr->dst==nullptr)  continue;

      //origin list
      TEMP::TempList *src=operinstr->src;
      TEMP::TempList *dst=operinstr->dst;
      //new list
      TEMP::TempList *new_src;
      TEMP::TempList *new_dst;

      if(src)
      {
        std::stringstream stream;
        stream<<-(src->head->Int()-100)*F::wordSize<<"(%rbp)";
        map->Enter(src->head,new std::string(stream.str()));
        new_src=new TEMP::TempList(F::R8(),nullptr);
        emit(new AS::MoveInstr("movq `s0 `d0",new TEMP::TempList(F::R8(),nullptr),new TEMP::TempList(src->head,nullptr)));
        src=src->tail;
      }
      if(src)
      {
        std::stringstream stream;
        stream<<-(src->head->Int()-100)*F::wordSize<<"(%rbp)";
        map->Enter(src->head,new std::string(stream.str()));
        new_src=new TEMP::TempList(F::RDX(),nullptr);
        new_src=new TEMP::TempList(F::R8(),new_src);
        emit(new AS::MoveInstr("movq `s0 `d0",new TEMP::TempList(F::RDX(),nullptr),new TEMP::TempList(src->head,nullptr)));
        src=src->tail;
      }  
      assert(src==nullptr);

      if(dst)
      {
        std::stringstream stream;
        stream<<-(dst->head->Int()-100)*F::wordSize<<"(%rbp)";
        map->Enter(dst->head,new std::string(stream.str()));
        new_dst=new TEMP::TempList(F::RAX(),nullptr);
      }

      emit(new AS::OperInstr(operinstr->assem,new_dst,new_src,operinstr->jumps));
      if(dst)
      {
          emit(new AS::MoveInstr("movq `s0 `d0",new_dst,dst));
      }
      
      

      
      break;
    }
    case AS::Instr::LABEL:
    {
      AS::LabelInstr *labelinstr=(AS::LabelInstr*) instrlist->head;
      emit(labelinstr);
      break;
    }
    case AS::Instr::MOVE:
    {
      AS::MoveInstr *moveinstr=(AS::MoveInstr *)instrlist->head;
      // moveinstr->
      TEMP::TempList *src=moveinstr->src;
      TEMP::TempList *dst=moveinstr->dst;
      while(src)
      {
        assert(src);
        //std::string *stackposition=std::to_string(src->head->Int()*F::wordSize);
        std::string stack_position=std::to_string(src->head->Int()*F::wordSize);
        stack_position+= "(rbp)";
        std::stringstream stream;
        stream<<-(src->head->Int()-100)*F::wordSize<<"(%rbp)";
        map->Enter(src->head,new std::string(stream.str()));
        src=src->tail;
      }
      while(dst)
      {
        assert(dst);
        //std::string *stackposition=std::to_string(src->head->Int()*F::wordSize);
        std::string stack_position=std::to_string(dst->head->Int()*F::wordSize);
        stack_position+= "(rbp)";
        std::stringstream stream;
        stream<<-(dst->head->Int()-100)*F::wordSize<<"(%rbp)";
        map->Enter(dst->head,new std::string(stream.str()));
        dst=dst->tail;
      }
      assm="movq `s0 `d0";
      emit(new AS::MoveInstr(assm,new TEMP::TempList(F::RCX(),nullptr),moveinstr->src));
      emit(new AS::MoveInstr(assm,moveinstr->dst,new TEMP::TempList(F::RCX(),nullptr)));
      break;
    }
    default:
    assert(0);
      break;
    }
  }
  return result;
}

}  // namespace RA