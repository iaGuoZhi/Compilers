#include "tiger/frame/temp.h"

#include <cstdio>
#include <sstream>

namespace {

int labels = 0;
static int temps = 100;
FILE *outfile;

void showit(TEMP::Temp *t, std::string *r) {
  fprintf(outfile, "t%d -> %s\n", t->Int(), r->c_str());
}

}  // namespace

namespace TEMP {

Label *NewLabel() {
  char buf[100];
  sprintf(buf, "L%d", labels++);
  return NamedLabel(std::string(buf));
}

/* The label will be created only if it is not found. */
Label *NamedLabel(std::string s) { return S::Symbol::UniqueSymbol(s); }

const std::string &LabelString(Label *s) { return s->Name(); }

Temp *Temp::NewTemp() {
  Temp *p = new Temp(temps++);
  std::stringstream stream;
  stream << p->num;
  Map::Name()->Enter(p, new std::string(stream.str()));
  return p;
}

int Temp::Int() { return this->num; }

Map *Map::Empty() { return new Map(); }

Map *Map::Name() {
  static Map *m = nullptr;
  if (!m) m = Empty();
  //m->>Enter(F::RBP(),new std::string("%rbp"));
  return m;
}

Map *Map::LayerMap(Map *over, Map *under) {
  if (over == nullptr)
    return under;
  else
    return new Map(over->tab, LayerMap(over->under, under));
}

void Map::Enter(Temp *t, std::string *s) {
  assert(this->tab);
  this->tab->Enter(t, s);
}

std::string *Map::Look(Temp *t) {
  std::string *s;
  assert(this->tab);
  s = this->tab->Look(t);
  if (s)
    return s;
  else if (this->under)
    return this->under->Look(t);
  else
    return nullptr;
}

void Map::DumpMap(FILE *out) {
  outfile = out;
  this->tab->Dump((void (*)(Temp *, std::string *))showit);
  if (this->under) {
    fprintf(out, "---------\n");
    this->under->DumpMap(out);
  }
}

bool tempEqual(TEMP::TempList *t1,TEMP::TempList *t2)
{
  TEMP::TempList *p=t1;
  for(p;p;p=p->tail)
  {
    if(!inList(t2,p->head))
    {
      return false;
    }
  }

  p=t2;
  for(p;p;p=p->tail)
  {
    if(!inList(t1,p->head))
    {
      return false;
    }
  }
  return true;
}

bool inList(TEMP::TempList *t1,TEMP::Temp *t)
{
  assert(t);
  if(t1==nullptr)
    return false;
  if(t1->head==t)
    return true;
  return inList(t1->tail,t);
}

TempList *unionTempList(TempList *t1,TempList *t2)
{
  TempList *insertList=t2;
  TempList *baseList=t1;
  for(insertList;insertList;insertList=insertList->tail)
  {
    if(inList(baseList,insertList->head))
      continue;
    baseList=new TempList(insertList->head,baseList);
  }
  return baseList;  
}

TempList *subTempList(TempList *t1,TempList *t2)
{
  TempList *baseListLast=t1;
  TempList *baseList=nullptr;
  for(baseListLast;baseListLast!=nullptr;baseListLast=baseListLast->tail)
  {
    TEMP::Temp *test=baseListLast->head;
    if(inList(t2,baseListLast->head))
    {
      continue;
    }else
    {
    baseList=new TempList(baseListLast->head,baseList);
    }
  }
  return baseList;
}

void printList(TEMP::TempList *t)
{
  while(t)
  {
    printf("%d\t",t->head->Int());
    t=t->tail;
  }
  printf("\n");
}

}  // namespace TEMP