%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
digit [0-9]
letter [a-z]|[A-Z]
id {letter}({letter}|{digit}|_)*
int {digit}+
%x COMMENT STR 
%%

 /*
  * skip white space chars.
  * space, tabs and LF
  */
  [ \t]+ {adjust();}
\n {adjust(); errormsg.Newline();}

  /*comment_case*/
  "/*" {adjust();commentPlus();}
  "*/" {adjust(); commentCut();}



 /* reserved words */
<INITIAL>"array" {adjust(); if(comment_count==0) return Parser::ARRAY;}
<INITIAL>"if" {adjust();if(comment_count==0) return Parser::IF;}
<INITIAL>"then" {adjust(); if(comment_count==0)return Parser::THEN;}
<INITIAL>"else" {adjust(); if(comment_count==0)return Parser::ELSE;}
<INITIAL>"while" {adjust();if(comment_count==0) return Parser::WHILE;}
<INITIAL>"for" {adjust(); if(comment_count==0)return Parser::FOR;}
<INITIAL>"to" {adjust(); if(comment_count==0)return Parser::TO;}
<INITIAL>"do" {adjust(); if(comment_count==0)return Parser::DO;}
<INITIAL>"let" {adjust(); if(comment_count==0)return Parser::LET;}
<INITIAL>"in" {adjust(); if(comment_count==0)return Parser::IN;}
<INITIAL>"end" {adjust(); if(comment_count==0)return Parser::END;}
<INITIAL>"of" {adjust(); if(comment_count==0)return Parser::OF;}
<INITIAL>"break" {adjust();if(comment_count==0) return Parser::BREAK;}
<INITIAL>"nil" {adjust();if(comment_count==0) return Parser::NIL;}
<INITIAL>"function" {adjust(); if(comment_count==0)return Parser::FUNCTION;}
<INITIAL>"var" {adjust();if(comment_count==0) return Parser::VAR;}
<INITIAL>"type" {adjust(); if(comment_count==0)return Parser::TYPE;}


/* regular expressions */
<INITIAL>{id} {adjust(); if(comment_count==0)return Parser::ID;}
<INITIAL>{int} {adjust();if(comment_count==0) return Parser::INT;}
/*symbol*/
<INITIAL>',' {adjust(); if(comment_count==0)return Parser::COMMA;}
<INITIAL>':' {adjust(); if(comment_count==0)return Parser::COLON;}
<INITIAL>';' {adjust(); if(comment_count==0)return Parser::SEMICOLON;}
<INITIAL>'(' {adjust();if(comment_count==0) return Parser::LPAREN;}
<INITIAL>')' {adjust(); if(comment_count==0)return Parser::RPAREN;}
<INITIAL>'[' {adjust(); if(comment_count==0)return Parser::LBRACK;}
<INITIAL>']' {adjust(); if(comment_count==0)return Parser::RBRACK;}
<INITIAL>"{" {adjust(); if(comment_count==0)return Parser::LBRACE;}
<INITIAL>"}" {adjust(); if(comment_count==0)return Parser::RBRACE;}
<INITIAL>'.' {adjust(); if(comment_count==0)return Parser::DOT;}
<INITIAL>'+' {adjust(); if(comment_count==0)return Parser::PLUS;}
<INITIAL>'-' {adjust(); if(comment_count==0)return Parser::MINUS;}
<INITIAL>'*' {adjust();if(comment_count==0) return Parser::TIMES;}
<INITIAL>'/' {adjust(); if(comment_count==0)return Parser::DIVIDE;}
<INITIAL>'=' {adjust(); if(comment_count==0)return Parser::EQ;}
<INITIAL>"<>" {adjust(); if(comment_count==0)return Parser::NEQ;}
<INITIAL>"<=" {adjust(); if(comment_count==0)return Parser::LE;}
<INITIAL>">=" {adjust(); if(comment_count==0)return Parser::GE;}
<INITIAL>'<' {adjust(); if(comment_count==0)return Parser::LT;}
<INITIAL>'>' {adjust(); if(comment_count==0)return Parser::GT;}
<INITIAL>'&' {adjust(); if(comment_count==0)return Parser::AND;}
<INITIAL>'|' {adjust(); if(comment_count==0)return Parser::OR;}
<INITIAL>":=" {adjust();if(comment_count==0) return Parser::ASSIGN;}

/* handle string*/
<INITIAL>\"  {
  adjust();
  if(comment_count==0)
  begin(StartCondition__::STR);
}
<STR>\" {
  adjustStr();
  setMatched(stringBuf_);
  stringBuf_="";
  begin(StartCondition__::INITIAL);
  return Parser::STRING;
}

<STR>"\\t" {
  adjustStr();
  stringBuf_+="\t";
}

<STR>\\{digit}{digit}{digit} {
  adjustStr();
  std::string parseStringToAscall=matched();
  int num;
  num=(parseStringToAscall[1]-'0')*100+(parseStringToAscall[2]-'0')*10+parseStringToAscall[3]-'0';
  stringBuf_+=('a'+num-97);

}

<STR>(\\\^[A-Z]) {
  adjustStr();
  stringBuf_+=(matched()[2]-'A'+1);
}

<STR>"\\n" {
  adjustStr();
  stringBuf_+='\n';
}

<STR>"\\\"" {
  adjustStr();
  stringBuf_+='\"';
}

<STR>"\\\\" {
  adjustStr();
  stringBuf_+="\\";
}

<STR>\\[ \n\t\f]+\\ {
  adjustStr();

}

<STR>. {
  adjustStr();
  stringBuf_+=matched();
}

. {adjust(); 
if(comment_count==0) {errormsg.Error(errormsg.tokPos, "illegal token");}
}
