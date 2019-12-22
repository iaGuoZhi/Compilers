#include "tiger/liveness/liveness.h"

namespace LIVE {
bool inMoveList(G::Node<TEMP::Temp>* src, G::Node<TEMP::Temp>* dst, MoveList* movelist)
{
  assert(src);
  assert(dst);
  if(movelist==nullptr)
    return false;
  if(movelist->dst==dst&&movelist->src==src)
    return true;
  return inMoveList(src,dst,movelist->tail);
}

MoveList *unionMoveList(MoveList *a,MoveList *b)
{
  MoveList *baseList=a;
  MoveList *insertList=b;
  for(insertList;insertList;insertList=insertList->tail)
  {
    if(inMoveList(insertList->src,insertList->dst,baseList))
      continue;
    baseList=new MoveList(insertList->src,insertList->dst,baseList);
  }
  return baseList;
}

MoveList *subMoveList(MoveList *a,MoveList *b)
{
  MoveList *baseListLast=a;
  MoveList *baseList=nullptr;
  for(baseListLast;baseListLast;baseListLast=baseListLast->tail)
  {
    if(inMoveList(baseListLast->src,baseListLast->dst,b))
      continue;
    baseList=new MoveList(baseListLast->src,baseListLast->dst,baseList);
  }
  return baseList;
}
//draw the interference graph
LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph) {
  G::Graph<TEMP::Temp>*interferenceGraph=new G::Graph<TEMP::Temp>();
  G::NodeList<AS::Instr> *nodes=flowgraph->mynodes,*succs=nullptr;
  G::NodeList<TEMP::Temp>*tempNodes=nullptr,*tempNodes2=nullptr;
  G::Node<AS::Instr> *node=nullptr;
  G::Table<AS::Instr,TEMP::TempList > *inTable=new G::Table<AS::Instr,TEMP::TempList>(),*outTable=new G::Table<AS::Instr,TEMP::TempList>();
  TAB::Table<TEMP::Temp,G::Node<TEMP::Temp>> *tempTable=new TAB::Table<TEMP::Temp,G::Node<TEMP::Temp>>();
  MoveList* moves=nullptr;
  LiveGraph resultGraph;
  //is a iteration change inTable or outTable
  bool changed=false;

  //build in&out table
  do{
    changed=false;
    nodes=flowgraph->mynodes;
    //iteration
    for(nodes;nodes;nodes=nodes->tail)
    {
      node=nodes->head;
      //in`[n]<-in[n]
      TEMP::TempList *in_prior=inTable->Look(node);
      //out`[n]-<out[n]
      TEMP::TempList *out_prior=outTable->Look(node);
      TEMP::TempList *out=nullptr,*in=nullptr;
      //in[n]<-(use[n]||(out[n]-def[n]))
      in=TEMP::unionTempList(FG::Use(node),TEMP::subTempList(out_prior,FG::Def(node)));
      //out[n]<- all in[succ(n)]'s Union
      succs=node->Succ();
      for(succs;succs;succs=succs->tail)
      {
        TEMP::TempList *in_succs=inTable->Look(succs->head);
        out=TEMP::unionTempList(out,in_succs);
      }
      inTable->Enter(node,in);
      outTable->Enter(node,out);
      if(!(TEMP::tempEqual(in,in_prior)&&TEMP::tempEqual(out,out_prior)))
        changed=true;
    }
  }
  while(changed);
  
  //build interference graph

  //put hard register into interfernce graph
  TEMP::TempList *hardReg=F::hardReg();
  for(hardReg;hardReg;hardReg=hardReg->tail)
  {
    interferenceGraph->NewNode(hardReg->head);
  }
  //ligature hard registers
  tempNodes=interferenceGraph->Nodes();
  for(;tempNodes;tempNodes=tempNodes->tail)
  {
    tempNodes2=tempNodes->tail;
    for(;tempNodes2;tempNodes2=tempNodes2->tail)
    {
      interferenceGraph->AddEdge(tempNodes->head,tempNodes2->head);
      interferenceGraph->AddEdge(tempNodes2->head,tempNodes->head);
    }
  }
  //put temp into interference graph
  nodes=flowgraph->mynodes;
  for(nodes;nodes;nodes=nodes->tail)
  {
    node=nodes->head;
    TEMP::TempList *in=inTable->Look(node);
    TEMP::TempList *out=outTable->Look(node);
    TEMP::TempList *def=FG::Def(node);
    TEMP::TempList *outUnionDef=TEMP::unionTempList(TEMP::unionTempList(out,def),in);
    for(;outUnionDef;outUnionDef=outUnionDef->tail)
    {
      if(tempTable->Look(outUnionDef->head)==nullptr)
      {
        G::Node<TEMP::Temp> *tempNode=interferenceGraph->NewNode(outUnionDef->head);
        tempTable->Enter(outUnionDef->head,tempNode);
      }
    }
  }

  //ligature temp
  nodes=flowgraph->mynodes;
  for(;nodes;nodes=nodes->tail)
  {
    node=nodes->head;
    TEMP::TempList *out=outTable->Look(node);
    TEMP::TempList *use=FG::Use(node);
    TEMP::TempList *def=FG::Def(node);
    /*
    *为每一个新定值添加冲突边的办法是：
    * 对于任何对变量a定值的非传送指令，以及在该指令是出口活跃的变量b，添加冲突边a，b
    * 对于传送指令a<-c,如果变量b出口活跃，则对每一个不同于c的b添加冲突边a,b
    * */
    if(FG::IsMove(node))
    {
       for(;def;def=def->tail)
       {
          if(def->head==F::FP())
            continue;
          G::Node<TEMP::Temp> *node1=tempTable->Look(def->head);
          for(out;out;out=out->tail)
          {
            if(out->head==F::FP()||out->head==def->head)
              continue;
            G::Node<TEMP::Temp> *node2=tempTable->Look(out->head);
            interferenceGraph->AddEdge(node1,node2);
            interferenceGraph->AddEdge(node2,node1);
          }

          //add move into movelist
          for(;use;use=use->tail)
          {
            if(use->head==F::FP()||use->head==def->head)
              continue;
            G::Node<TEMP::Temp> *node2=tempTable->Look(use->head);
            if(!inMoveList(node2,node1,moves))
            {
              moves=new MoveList(node2,node1,moves);
            }
          }
       }
    }else{
      for(;def;def=def->tail)
      {
        if(def->head==F::FP())
          continue;
        G::Node<TEMP::Temp> *node1=tempTable->Look(def->head);
        for(out;out;out=out->tail)
        {
          if(out->head==F::FP()||out->head==def->head)
            continue;
          G::Node<TEMP::Temp> *node2=tempTable->Look(out->head);
          interferenceGraph->AddEdge(node1,node2);
          interferenceGraph->AddEdge(node2,node1);
        }
      }
    }
    
  }
  interferenceGraph->Show(stdout,interferenceGraph->Nodes(),nullptr);
  resultGraph.graph=interferenceGraph;
  resultGraph.moves=moves;
  return resultGraph;
}

}  // namespace LIVE