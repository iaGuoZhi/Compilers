#include "tiger/liveness/flowgraph.h"

namespace FG {

TEMP::TempList* Def(G::Node<AS::Instr>* n) {
  // TODO: Put your codes here (lab6).
  switch (n->NodeInfo()->kind)
  {
  case AS::Instr::MOVE:
    return ((AS::MoveInstr*)n->NodeInfo())->dst;
  case AS::Instr::OPER:
    return ((AS::OperInstr*)n->NodeInfo())->dst;
  default:
    return nullptr;
  }
}

TEMP::TempList* Use(G::Node<AS::Instr>* n) {
  // TODO: Put your codes here (lab6).
  switch (n->NodeInfo()->kind)
  {
  case AS::Instr::MOVE:
    return ((AS::MoveInstr*)n->NodeInfo())->src;
  case AS::Instr::OPER:
    return ((AS::OperInstr*)n->NodeInfo())->src;
  default:
    return nullptr;
  }
}

bool IsMove(G::Node<AS::Instr>* n) {
  // TODO: Put your codes here (lab6).
  return n->NodeInfo()->kind==AS::Instr::MOVE;
}

G::Graph<AS::Instr>* AssemFlowGraph(AS::InstrList* il, F::Frame* f) {
  // TODO: Put your codes here (lab6).
  G::Graph<AS::Instr > *instrGraph=new G::Graph<AS::Instr>();
  AS::InstrList *instrList=il;
  G::NodeList<AS::Instr> *assemNodeList=nullptr,*labelNodeList=nullptr;
  AS::Instr *instr=nullptr,*operinstr=nullptr;
  AS::OperInstr *operInstr=nullptr;
  AS::LabelInstr *labelInstr=nullptr;

  //put instrs in the graph
  for(instrList;instrList;instrList=instrList->tail)
  {
    instr=instrList->head;
    instrGraph->NewNode(instr);
  }

  //add edges between adjacent instrs
  assemNodeList=instrGraph->Nodes();
  assert(assemNodeList);
  for(assemNodeList;assemNodeList;assemNodeList=assemNodeList->tail)
  {
    //sequence instr
    if(assemNodeList->tail)
    {
      instrGraph->AddEdge(assemNodeList->head,assemNodeList->tail->head);
    }
    
    //jump & cjump instr
    if(assemNodeList->head->NodeInfo()->kind==AS::Instr::OPER)
    {
      operinstr=assemNodeList->head->NodeInfo();
      operInstr=(AS::OperInstr *)(operinstr);
      if(operInstr->jumps&&operInstr->jumps->labels&&operInstr->jumps->labels->head)
      {
       //look up jump label instr
        labelNodeList=instrGraph->Nodes();
        for(labelNodeList;labelNodeList; labelNodeList=labelNodeList->tail)
        {
          if(labelNodeList->head->NodeInfo()->kind==AS::Instr::LABEL)
          {
            labelInstr=(AS::LabelInstr *)labelNodeList->head->NodeInfo();
            if(labelInstr->label==operInstr->jumps->labels->head)
            {
              break;
            }
          }
        }
        assert(labelNodeList);
        assert(labelInstr);
        assert(labelInstr->label==operInstr->jumps->labels->head);
        instrGraph->AddEdge(assemNodeList->head,labelNodeList->head);
      }
    }
  }
  //instrGraph->Show(stdout,instrGraph->Nodes(),nullptr);
  return instrGraph;
}

}  // namespace FG
