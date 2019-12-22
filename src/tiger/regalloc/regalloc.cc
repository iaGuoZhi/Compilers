#include "tiger/regalloc/regalloc.h"

#define K 14
#define RegNode G::Node<TEMP::Temp>
#define RegNodeList G::NodeList<TEMP::Temp>

namespace RA {
static  void MakeWorklist();
static  void Build();
static  void Simplify();/* condition */
static  void Coalesce();
static  void Freeze();
static  void AssignColors();
static  void AddEdge(G::Node<TEMP::Temp> *u,G::Node<TEMP::Temp> *v);
static  bool precolored(G::Node<TEMP::Temp>*);
static bool MoveRelated(G::Node<TEMP::Temp> *t);
static RegNodeList *Adjacent(G::Node<TEMP::Temp>n);
static LIVE::MoveList *NodeMoves(RegNode *n);
static RegNode *GetAlias(RegNode *n);
static bool MoveRelated(RegNode *n);
static void DecrementDegree(RegNode *m);
static void EnableMoves(RegNodeList *nodelist);
static void AddWorkList(RegNode *n);
static bool OK(RegNode *u,RegNode *v);
static bool Conservative(RegNodeList *nodes);
static void Combine(RegNode *u,RegNode *v);
static void FreezeMoves(RegNode *u);
static void SelectSpill();
static void AssignColors();
static TEMP::Map *AssignRegisters(LIVE::LiveGraph graph);
static void RewriteProgram(F::Frame *frame,AS::InstrList *instrlist);

static LIVE::LiveGraph liveGraph;
//低度数传送无关的节点
static RegNodeList *simplifyWorklist=nullptr;

//低度数的传送有关的节点
static RegNodeList *freezeWorklist=nullptr;
//高度数的节点
static RegNodeList *spillWorklist=nullptr;


//确认要spill的节点
static RegNodeList *spilledNodes=nullptr;

static G::Table<TEMP::Temp,int> *degreeTable=new TAB::Table<G::Node<TEMP::Temp>,int>();
static G::Table<TEMP::Temp,int> *colorTable=new TAB::Table<G::Node<TEMP::Temp>,int>();
static G::Table<TEMP::Temp,RegNode> *aliasTable=new TAB::Table<G::Node<TEMP::Temp>,RegNode>();
static RegNodeList *selectStack=nullptr;
static RegNodeList *coalescedNodes=nullptr;

static TEMP::TempList *unSpillTemps=nullptr;

//传送指令相关的数据结构
static LIVE::MoveList *coalescedMoves=nullptr;
static LIVE::MoveList *constrainedMoves=nullptr;
static LIVE::MoveList *frozenMoves=nullptr;
static LIVE::MoveList *worklistMoves=nullptr;
static LIVE::MoveList *activeMoves=nullptr;
Result RegAlloc(F::Frame* f, AS::InstrList* il) {
 
  do{
    //draw control flow graph instruction-by-instruction
    G::Graph<AS::Instr> *instrGraph=FG::AssemFlowGraph(il,f);
    //analysis liveness
    liveGraph=LIVE::Liveness(instrGraph);
    Build();
    MakeWorklist();
    
    do{
      if(simplifyWorklist!=nullptr)
      {
        Simplify();
      }
      if(worklistMoves!=nullptr)
      {
        Coalesce();
      }
      if(freezeWorklist!=nullptr)
      {
        Freeze();
      }
      if(spillWorklist!=nullptr)
      {
        SelectSpill();
      }
      if(simplifyWorklist==nullptr&&worklistMoves==nullptr&&freezeWorklist==nullptr&&spillWorklist==nullptr)
        break;
    }
    while(true);
    AssignColors();
    if(spilledNodes==nullptr)
      break;
    else{
      RegNodeList *nodes=spilledNodes;
      for(nodes;nodes;nodes=nodes->tail)
      {
        TEMP::Temp *temp=nodes->head->NodeInfo();
        fprintf(stdout,"node t%d\n",temp->Int());
      }
      RewriteProgram(f,il);
    }
  }
  while(true);
  RA::Result result;
  result.coloring=AssignRegisters(liveGraph);
  result.il=il;
  return result;
}

//过程build使用静态活跃分析的结果来构造冲突图和位矩阵，并且初始化worklistMoves，使之包含程序中所有的传送指令。
static void Build(){
  simplifyWorklist=nullptr;
  freezeWorklist=nullptr;
  spillWorklist=nullptr;
  
  spilledNodes=nullptr;
  coalescedNodes=nullptr;
  selectStack=nullptr;

  coalescedMoves=nullptr;
  constrainedMoves=nullptr;
  frozenMoves=nullptr;
  worklistMoves=liveGraph.moves;
  activeMoves=nullptr;

  RegNodeList *nodes=liveGraph.graph->Nodes();
  for(nodes;nodes;nodes=nodes->tail)
  {
    int *degree=(int *)malloc(sizeof(int));
    int *color=(int *)malloc(sizeof(int));
    *degree=0;
    *color=0;
    for(RegNodeList *cur=nodes->head->Succ();cur;cur=cur->tail)
    {
      (*degree)++;
    }
    degreeTable->Enter(nodes->head,degree);

    TEMP::Temp *temp=nodes->head->NodeInfo();
    TEMP::TempList *hardRegs=F::hardReg();
    if(TEMP::inList(hardRegs,temp))
    {
      //color is the index of hardreg
      (*color)++;
      while(temp!=hardRegs->head)
      {
        (*color)++;
        hardRegs=hardRegs->tail;
      }
    }
    fprintf(stdout,"color %d\n",*color);
    colorTable->Enter(nodes->head,color);
    fprintf(stdout,"%d\n",*colorTable->Look(nodes->head));

    RegNode *alias=(RegNode *)malloc(sizeof(RegNode));
    alias=nodes->head;
    aliasTable->Enter(nodes->head,alias);
  }
}

static void AddEdge(G::Node<TEMP::Temp> *u,G::Node<TEMP::Temp> *v)
{
  if(u->Adj()->InNodeList(v)==false&&u!=v)
  {
    if(!precolored(u)){
      (*(int *)degreeTable->Look(u))++;
      liveGraph.graph->AddEdge(u,v);
    }
    if(!precolored(v)){
      (*(int *)degreeTable->Look(v))++;
      liveGraph.graph->AddEdge(v,u);
    }
  }
}

static void MakeWorklist()
{
  RegNodeList *nodes=liveGraph.graph->Nodes();
  for(nodes;nodes;nodes=nodes->tail)
  {
    int *degree=degreeTable->Look(nodes->head);
    int *color=colorTable->Look(nodes->head);
    fprintf(stdout,"makeworklist %d\n",*color);
    //hard register
    
    if(*color!=0){
      continue;
    }
    //spill
    if(*degree>=K)
    {
      spillWorklist=new RegNodeList(nodes->head,spillWorklist);
    }
    else if(MoveRelated(nodes->head)){
      freezeWorklist=new RegNodeList(nodes->head,freezeWorklist);
    }
    //simplify
    else{
      simplifyWorklist=new RegNodeList(nodes->head,simplifyWorklist);
    }
  }
}

static RegNodeList *Adjacent(RegNode *n)
{
  return G::subNodeList(G::subNodeList(n->Succ(),selectStack),coalescedNodes);
}
// movelist[n]&&(actieMoves||worklistMoves)
static LIVE::MoveList *NodeMoves(RegNode *n)
{
  LIVE::MoveList *moves=nullptr,*p=nullptr;
  RegNode *m=GetAlias(n);
  p=LIVE::unionMoveList(activeMoves,worklistMoves);
  for(;p;p=p->tail)
  {
    if(GetAlias(p->src)==m||GetAlias(p->dst)==m)
    {
      moves=new LIVE::MoveList(p->src,p->dst,moves);
    }
  }
  return moves;
}

static bool MoveRelated(RegNode *n)
{
  return (NodeMoves(n)!=nullptr);
}

static void Simplify(){
  RegNode *n=simplifyWorklist->head;
  simplifyWorklist=simplifyWorklist->tail;
  selectStack=new RegNodeList(n,selectStack);

  //去掉一个节点需要减少该节点的当前各个邻节点的度数
  for(RegNodeList *nodes=Adjacent(n);nodes;nodes=nodes->tail)
  {
    DecrementDegree(nodes->head);
  }
}

static void DecrementDegree(RegNode *m)
{
  int *degree=degreeTable->Look(m);
  int d=*degree;
  (*degree)--;
  int *color=colorTable->Look(m);
  if(d==K&&*color==0)
  {
    EnableMoves(G::unionNodeList(new RegNodeList(m,nullptr),Adjacent(m)));
    spillWorklist=G::subNodeList(spillWorklist,new RegNodeList(m,nullptr));
    if(MoveRelated(m))
    {
      freezeWorklist=G::unionNodeList(freezeWorklist,new RegNodeList(m,nullptr));
    }else{
      simplifyWorklist=G::unionNodeList(simplifyWorklist,new RegNodeList(m,nullptr));
    }
  }
}

static void EnableMoves(RegNodeList *nodes)
{
  for(nodes;nodes;nodes=nodes->tail)
  {
    LIVE::MoveList *mlist=NodeMoves(nodes->head);
    for(mlist;mlist;mlist=mlist->tail)
    {
      if(LIVE::inMoveList(mlist->src,mlist->dst,activeMoves))
      {
        activeMoves=LIVE::subMoveList(activeMoves,new LIVE::MoveList(mlist->src,mlist->dst,nullptr));
        worklistMoves=LIVE::unionMoveList(worklistMoves,new LIVE::MoveList(mlist->src,mlist->dst,nullptr));
      }
    }
  }
}

//合并阶段只考虑worklistMoves中的传送指令
static void Coalesce(){
  assert(worklistMoves);
  LIVE::MoveList *m=new LIVE::MoveList(worklistMoves->src,worklistMoves->dst,nullptr);
  worklistMoves=worklistMoves->tail;
  RegNode *u=nullptr,*v=nullptr;
  RegNode *x=m->src;
  RegNode *y=m->dst;
  x=GetAlias(x);
  y=GetAlias(y);
  if(precolored(y))
  {
    u=y;
    v=x;
  }else{
    u=x;
    v=y;
  }
  // u,v the same temp
  if(u==v)
  {
    coalescedMoves=LIVE::unionMoveList(coalescedMoves,m);
    AddWorkList(u);
  }else{
    //v is hard reg,but u,v not only move related, but also adjacent
    if(precolored(v)||(v->Adj()->InNodeList(u)))
    {
      constrainedMoves=LIVE::unionMoveList(constrainedMoves,m);
      AddWorkList(u);
      AddWorkList(v);
    }else{
      //满足George或Briggs
      if(precolored(u)&&(OK(v,u))||(!precolored(u)&&Conservative(G::unionNodeList(Adjacent(u),Adjacent(v))))){
        coalescedMoves=LIVE::unionMoveList(coalescedMoves,m);
        Combine(u,v);
        AddWorkList(u);
      }else{
        activeMoves=LIVE::unionMoveList(activeMoves,m);
      }
    }
  }
}

//从传送有关的低度数节点添加到传送无关的低度数节点集合
static void AddWorkList(RegNode *n)
{
  if(!precolored(n)&&!MoveRelated(n)&&*degreeTable->Look(n)<K)
  {
    freezeWorklist=G::subNodeList(freezeWorklist,new RegNodeList(n,nullptr));
    simplifyWorklist=new RegNodeList(n,simplifyWorklist);
  }
}

//George coalesce strategy
static bool OK(RegNode *u,RegNode *v)
{
  for(RegNodeList *nodes=Adjacent(u);nodes;nodes=nodes->tail)
  {
    if(*(int*)degreeTable->Look(nodes->head)<K||precolored(nodes->head)||v->Adj()->InNodeList(nodes->head))
      continue;
    else
    {
      return false;
    }
  }
  return true;
}

//Briggs coalesce strategy
static bool Conservative(RegNodeList *nodes)
{
  int k=0;
  for(nodes;nodes;nodes=nodes->tail)
  {
    if(*degreeTable->Look(nodes->head)>=K)
      k++;
  }
  return k<K;
}

static RegNode *GetAlias(RegNode *n)
{
  RegNode *res=aliasTable->Look(n);
  if(res!=n)
  {
    res=GetAlias(res);
  }
  return res;
}

//合并操作
static void Combine(RegNode *u,RegNode *v)
{
  //combine 时，v为何会出现在这两个集合中？？？
  if(freezeWorklist->InNodeList(v))
  {
    freezeWorklist=G::subNodeList(freezeWorklist,new RegNodeList(v,nullptr));
  }else{
    spillWorklist=G::subNodeList(spillWorklist,new RegNodeList(v,nullptr));
  }

  coalescedNodes=new RegNodeList(v,coalescedNodes);
  aliasTable->Set(v,u);
  for(RegNodeList *nodes=v->Adj();nodes;nodes=nodes->tail)
  {
    AddEdge(nodes->head,u);
    DecrementDegree(nodes->head);
  }
  if(*(int*)degreeTable->Look(u)>=K&&freezeWorklist->InNodeList(u)){
    freezeWorklist=G::subNodeList(freezeWorklist,new RegNodeList(u,nullptr));
    spillWorklist=new RegNodeList(u,spillWorklist);
  }
} 

static void Freeze(){
  RegNode *u=freezeWorklist->head;
  freezeWorklist=freezeWorklist->tail;
  simplifyWorklist=new RegNodeList(u,simplifyWorklist);
  FreezeMoves(u);
}

//冻结掉与节点相关的所有传送指令
static void FreezeMoves(RegNode *u)
{
  for(LIVE::MoveList *m=NodeMoves(u);m;m=m->tail)
  {
    RegNode *x=m->src;
    RegNode *y=m->dst;
    RegNode *v=nullptr;
    if(GetAlias(y)==GetAlias(u)){
      v=GetAlias(x);
    }
    else{
      v=GetAlias(y);
    }
    activeMoves=LIVE::subMoveList(activeMoves,new LIVE::MoveList(x,y,nullptr));
    frozenMoves=new LIVE::MoveList(x,y,frozenMoves);
    if(NodeMoves(v)==nullptr&&*(int*)degreeTable->Look(v)<K)
    {
      freezeWorklist=G::subNodeList(freezeWorklist,new RegNodeList(v,nullptr));
      simplifyWorklist=new RegNodeList(v,simplifyWorklist);
    }
  }
}

static void SelectSpill(){
  RegNode *spillNode=nullptr;
  int max=0;
  for(RegNodeList *p=spillWorklist;p;p=p->tail)
  {
    TEMP::Temp *temp=p->head->NodeInfo();

    // 要避免选择哪中由读取前面已经溢出的寄存器产生的活跃范围很小的节点
    if(TEMP::inList(unSpillTemps,temp)){
      continue;
    }
    int degree=*(int*)degreeTable->Look(p->head);
    if(degree>max)
    {
      max=degree;
      spillNode=p->head;
    }
  }

  if(!spillNode)
  {
    spillNode=spillWorklist->head;
  }

  spillWorklist=G::subNodeList(spillWorklist,new RegNodeList(spillNode,nullptr));
  simplifyWorklist=new RegNodeList(spillNode,simplifyWorklist);
  FreezeMoves(spillNode);
}

static void AssignColors(){
    bool okColors[K+1];

    assert(spilledNodes==nullptr);
    while(selectStack){
      okColors[0]=false;
      for(int i=1;i<=K;++i)
      {
        okColors[i]=true;
      }

      RegNode *spilledNode=selectStack->head;
      selectStack=selectStack->tail;
      for(RegNodeList *succs=spilledNode->Succ();succs;succs=succs->tail)
      {
        int *color=colorTable->Look(GetAlias(succs->head));
        printf("assigncolor %d\n",*color);
        assert(*color<F::hardRegSize());
        okColors[*color]=false;
      }

      int *i=(int*)malloc(sizeof(int));
      bool realSpill=true;
      for(*i=1;*i<=K;++(*i))
      {
        if(okColors[*i])
        {
          realSpill=false;
          break;
        }
      }
      
      if(realSpill){
        spilledNodes=new RegNodeList(spilledNode,spilledNodes);
      }else{
        colorTable->Set(spilledNode,i);
      }
    }
}

static bool precolored(RegNode *node)
{
  return colorTable->Look(node);
}

static TEMP::Map *AssignRegisters(LIVE::LiveGraph graph)
{
  TEMP::Map *result=TEMP::Map::Empty();
  F::add_register_to_map(result);
  RegNodeList *nodes=graph.graph->Nodes();


  for(;nodes;nodes=nodes->tail)
  {
    int *color=colorTable->Look(nodes->head);
    std::string name=F::hardRegsString(*color);
    result->Enter(nodes->head->NodeInfo(),new std::string(name));
  }
  result->Enter(F::FP(),new std::string("%r15"));
  result->Enter(F::RAX(),new std::string("%rax"));
  return result;
}

static void RewriteProgram(F::Frame *frame,AS::InstrList *instrlist)
{
  unSpillTemps=nullptr;
  AS::InstrList *il=instrlist,*l=nullptr,*last=nullptr,*next=nullptr,*new_instr=nullptr;
  int off; 
  assert(0);
  /*
  while(spilledNodes){
    RegNode *cur=spilledNodes->head;
    spilledNodes=spilledNodes->tail;
    TEMP::Temp *c=cur->NodeInfo();
    off=F::spill(frame);
    l=il;
    last=nullptr;
    while(l){
     // TEMP::Temp *

    }
  }*/
}

}  // namespace RA