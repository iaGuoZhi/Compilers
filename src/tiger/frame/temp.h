#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"

namespace TEMP {

using Label = S::Symbol;
//从标号的无穷集合中返回一个新的标号，在处理声明function f(...)中，通过调用Newlabel可生成
//f的机器代码地址的标号
Label *NewLabel();
//返回一个汇编名为name的新标号
Label *NamedLabel(std::string name);
const std::string &LabelString(Label *s);

class Temp {
 public:
  //从临时变量的无穷集合中返回一个新的临时变量
  static Temp *NewTemp();
  int Int();

 private:
  int num;
  Temp(int num) : num(num) {}
};

class Map {
 public:
  void Enter(Temp *t, std::string *s);
  std::string *Look(Temp *t);
  void DumpMap(FILE *out);

  static Map *Empty();
  static Map *Name();
  static Map *LayerMap(Map *over, Map *under);

 private:
  TAB::Table<Temp, std::string> *tab;
  Map *under;

  Map() : tab(new TAB::Table<Temp, std::string>()), under(nullptr) {}
  Map(TAB::Table<Temp, std::string> *tab, Map *under)
      : tab(tab), under(under) {}
};

class TempList {
 public:
  Temp *head;
  TempList *tail;

  TempList(Temp *h, TempList *t) : head(h), tail(t) {}
};
//t1+t2
TempList *unionTempList(TempList *t1,TempList *t2);
//t1-t2
TempList *subTempList(TempList *t1,TempList *t2);

bool tempEqual(TEMP::TempList *t1,TEMP::TempList *t2);

bool inList(TEMP::TempList *t1,TEMP::Temp *t);

void printList(TEMP::TempList *t);
class LabelList {
 public:
  Label *head;
  LabelList *tail;

  LabelList(Label *h, LabelList *t) : head(h), tail(t) {}
};

}  // namespace TEMP

#endif