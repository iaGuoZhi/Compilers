%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  S::Symbol *sym;
  A::Exp *exp;
  A::ExpList *explist;
  A::Var *var;
  A::DecList *declist;
  A::Dec *dec;
  A::EFieldList *efieldlist;
  A::EField *efield;
  A::NameAndTyList *tydeclist;
  A::NameAndTy *tydec;
  A::FieldList *fieldlist;
  A::Field *field;
  A::FunDecList *fundeclist;
  A::FunDec *fundec;
  A::Ty *ty;
  }

%token<sym> ID
%token <sval> STRING
%token <ival> INT

%token 
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK 
  LBRACE RBRACE DOT 
 ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE

%type <exp> exp expseq
%type <explist> actuals  nonemptyactuals sequencing  sequencing_exps
%type <var>  lvalue one oneormore
%type <declist> decs decs_nonempty
%type <dec>  decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec>  tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> field
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one

%start program


 /*
  * Put your codes here (lab3).
  */
%left AND OR
%left EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE


%%
program:  exp  {absyn_root = $1;};


lvalue: ID {$$ =new A::SimpleVar(errormsg.tokPos,$1);}
  | lvalue DOT ID {$$=new A::FieldVar(errormsg.tokPos,$1,$3);}
  | lvalue LBRACK exp RBRACK {$$=new A::SubscriptVar(errormsg.tokPos,$1,$3);}
  | ID LBRACK exp RBRACK {$$=new A::SubscriptVar(errormsg.tokPos,new A::SimpleVar(errormsg.tokPos,$1),$3);}
  ;

exp: lvalue {$$=new A::VarExp(errormsg.tokPos,$1);}
  | NIL {$$=new A::NilExp(errormsg.tokPos);}
  | INT {$$=new A::IntExp(errormsg.tokPos,$1);}
  | STRING {$$=new A::StringExp(errormsg.tokPos,$1);}
  | LPAREN expseq RPAREN {$$= $2;}
  | LPAREN exp RPAREN {$$=$2;}
  | LPAREN RPAREN {$$= new A::VoidExp(errormsg.tokPos);}
  | ID LPAREN RPAREN {$$=new A::CallExp(errormsg.tokPos,$1,nullptr);}
  | ID LPAREN actuals RPAREN {$$=new A::CallExp(errormsg.tokPos,$1,$3);}
  | exp EQ exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::EQ_OP,$1,$3);}
  | exp NEQ exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::NEQ_OP,$1,$3);}
  | exp PLUS exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::PLUS_OP,$1,$3);}
  | exp DIVIDE exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::DIVIDE_OP,$1,$3);}
  | exp TIMES exp{$$=new A::OpExp(errormsg.tokPos,A::Oper::TIMES_OP,$1,$3);}
  | exp MINUS exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::MINUS_OP,$1,$3);}
  | MINUS exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::MINUS_OP,new A::IntExp(errormsg.tokPos, 0),$2);}
  | exp LT exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::LT_OP,$1,$3);}
  | exp GT exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::GT_OP,$1,$3);}
  | exp LE exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::LE_OP,$1,$3);}
  | exp GE exp {$$=new A::OpExp(errormsg.tokPos,A::Oper::GE_OP,$1,$3);}
  | lvalue ASSIGN exp {$$=new A::AssignExp(errormsg.tokPos,$1,$3);}
  | exp AND exp {$$=new A::IfExp(errormsg.tokPos,$1,$3,new A::IntExp(errormsg.tokPos,0));}
  | exp OR exp {$$=new A::IfExp(errormsg.tokPos,$1,new A::IntExp(errormsg.tokPos,1),$3);}
  | IF exp THEN exp ELSE exp {$$=new A::IfExp(errormsg.tokPos,$2,$4,$6);}
  | IF exp THEN exp {$$=new A::IfExp(errormsg.tokPos,$2,$4,nullptr);}
  | ID LBRACE rec RBRACE {$$=new A::RecordExp(errormsg.tokPos,$1,$3);}
  | ID LBRACE RBRACE {$$=new A::RecordExp(errormsg.tokPos,$1,nullptr);}
  | WHILE exp DO exp {$$=new A::WhileExp(errormsg.tokPos,$2,$4);}
  | BREAK {$$=new A::BreakExp(errormsg.tokPos);}
  | FOR ID ASSIGN exp TO exp DO exp {$$=new A::ForExp(errormsg.tokPos,$2,$4,$6,$8);}
  | LET decs IN expseq END{$$=new A::LetExp(errormsg.tokPos,$2,$4);}
  | ID LBRACK exp RBRACK OF exp {$$=new A::ArrayExp(errormsg.tokPos,$1,$3,$6);}
  ;

sequencing: exp SEMICOLON sequencing {$$=new A::ExpList($1,$3);}
  | exp {$$=new A::ExpList($1,nullptr);}
  | exp SEMICOLON LPAREN RPAREN {$$=new A::ExpList($1,nullptr);}
  ;

expseq: sequencing {$$=new A::SeqExp(errormsg.tokPos,$1);}
  ;

ty: ARRAY OF ID {$$=new A::ArrayTy(errormsg.tokPos,$3);}
  | LBRACE tyfields RBRACE {$$=new A::RecordTy(errormsg.tokPos,$2);}
  | ID {$$=new A::NameTy(errormsg.tokPos,$1);}
  ;

decs: vardec decs {$$=new A::DecList($1,$2);}
  | vardec {$$=new A::DecList($1,nullptr);}
  ;

actuals: exp COMMA actuals {$$=new A::ExpList( $1,$3);}
  | exp {$$=new A::ExpList($1,nullptr);}
  ;

  
vardec:  VAR ID ASSIGN exp  {$$ = new A::VarDec(errormsg.tokPos,$2, nullptr, $4);}
  |  VAR ID COLON ID ASSIGN exp  {$$ = new A::VarDec(errormsg.tokPos, $2, $4, $6);}
  |  tydec {$$=new A::TypeDec(errormsg.tokPos, $1);}
  |  FUNCTION fundec {$$=new A::FunctionDec(errormsg.tokPos,$2);}
  ;

fundec: fundec_one FUNCTION fundec {$$=new A::FunDecList($1,$3);}
  | fundec_one {$$=new A::FunDecList($1,nullptr);}
  ;

fundec_one: ID LPAREN tyfields RPAREN COLON ID EQ exp {$$=new A::FunDec(errormsg.tokPos,$1,$3,$6,$8);}
  |  ID LPAREN tyfields RPAREN EQ exp {$$=new A::FunDec(errormsg.tokPos,$1,$3,nullptr,$6);}
  |  ID LPAREN RPAREN COLON ID EQ exp{$$=new A::FunDec(errormsg.tokPos,$1,nullptr,$5,$7);}
  | ID LPAREN RPAREN EQ exp {$$=new A::FunDec(errormsg.tokPos,$1,nullptr,nullptr,$5);}
  ;

tydec: tydec_one {$$=new A::NameAndTyList($1,nullptr);}
  | tydec_one tydec {$$=new A::NameAndTyList($1,$2);}
  ;

tydec_one: TYPE ID  EQ ty{$$=new A::NameAndTy($2,$4);}
  | TYPE ID COMMA ty{$$=new A::NameAndTy($2,$4);}
  ;

tyfields: field COMMA tyfields {$$=new A::FieldList($1,$3);}
  | field {$$=new A::FieldList($1,nullptr);}
  ;
field: ID COLON ID {$$=new A::Field(errormsg.tokPos,$1,$3);}
  ;

rec: rec_one COMMA rec {$$=new A::EFieldList($1,$3);}
  | rec_one {$$=new A::EFieldList($1,nullptr);}
  ;

rec_one: ID EQ exp {$$=new A::EField($1,$3);}
  ;

