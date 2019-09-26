#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <string>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

extern EM::ErrorMsg errormsg;


class Scanner : public ScannerBase {
 public:
  explicit Scanner(std::istream &in = std::cin, std::ostream &out = std::cout);

  Scanner(std::string const &infile, std::string const &outfile);

  int lex();

  

 private:
  int lex__();
  int executeAction__(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum__ type);
  void adjust();
  void back();
  void forward();
  void adjustStr();
  void commentPlus();
  void commentCut();

  int commentLevel_;
  std::string stringBuf_;
  int charPos_;
  int comment_count=0;
  
};

inline Scanner::Scanner(std::istream &in, std::ostream &out)
    : ScannerBase(in, out), charPos_(1) {}

inline Scanner::Scanner(std::string const &infile, std::string const &outfile)
    : ScannerBase(infile, outfile), charPos_(1) {}

inline int Scanner::lex() { return lex__(); }

inline void Scanner::preCode() {
  // optionally replace by your own code
}

inline void Scanner::postCode(PostEnum__ type) {
  // optionally replace by your own code
}

inline void Scanner::print() { print__(); }

inline void Scanner::adjust() {
  errormsg.tokPos = charPos_;
  charPos_ += length();
}

inline void Scanner::back(){
  charPos_-=1;
}

inline void Scanner::forward(){
  charPos_+=1;
}


inline void Scanner::adjustStr() { charPos_ += length(); }

inline void Scanner::commentPlus(){
  comment_count++;
}

inline void Scanner::commentCut(){
  comment_count--;
}




#endif  // TIGER_LEX_SCANNER_H_

