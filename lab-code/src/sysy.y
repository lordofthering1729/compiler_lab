%code requires {
  #include <memory>
  #include <string>
  #include <cassert>
  #include "AST.hpp"
  #include "sysy.tab.hpp"
}

%{

#include <cassert>
#include <iostream>
#include <memory>
#include "AST.hpp"
#include <string>

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
}

// token 声明，补充所有双字符运算符
%token INT RETURN
%token <str_val> IDENT
%token <int_val> INT_CONST
%token LE GE EQ NE AND OR

// 非终结符的类型定义
%type <ast_val> FuncDef FuncType Block Stmt Number Exp PrimaryExp UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp
%type <str_val> UnaryOp
%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
CompUnit
  : FuncDef {
    assert($1 != nullptr);
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def = unique_ptr<BaseAST>($1);
    ast = move(comp_unit);
  }
  ;

FuncDef
  : FuncType IDENT '(' ')' Block {
    assert($1 != nullptr);
    assert($2 != nullptr);
    assert($5 != nullptr);
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

FuncType
  : INT {
    //std::cerr << "[FuncType] INT ok\n";
    auto ast = new FuncTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  ;

Block
  : '{' Stmt '}' {
    assert($2 != nullptr);
    //std::cerr << "[Block] Stmt ok\n";
    auto ast = new BlockAST();
    ast->stmt = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

Stmt
  : RETURN Exp ';' {
    assert($2 != nullptr);
    //std::cerr << "[Stmt] Exp ok\n";
    auto ast = new StmtAST();
    ast->stmt = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

Exp
  : LOrExp { 
    assert($1 != nullptr);
    //std::cerr << "[Exp] LOrExp ok\n";
    auto ast = new ExpAST(); 
    ast->lor_exp = std::unique_ptr<BaseAST>($1); 
    $$ = ast;
  }
  ;

PrimaryExp
  : '(' Exp ')' { 
      assert($2 != nullptr);
      //std::cerr << "[PrimaryExp] (Exp) ok\n";
      auto ast = new PrimaryExpAST(); 
      ast->is_number = false; 
      ast->exp = std::unique_ptr<BaseAST>($2); 
      $$ = ast; 
    }
  | Number { 
      assert($1 != nullptr);
      //std::cerr << "[PrimaryExp] Number ok\n";
      auto ast = new PrimaryExpAST(); 
      ast->is_number = true; 
      ast->number_value = dynamic_cast<NumberAST*>($1)->value; 
      $$ = ast; 
    }
  ;

Number
  : INT_CONST { 
      //std::cerr << "[Number] INT_CONST=" << $1 << "\n";
      auto ast = new NumberAST(); 
      ast->value = $1; 
      $$ = ast; 
    }
  ;

UnaryExp
  : PrimaryExp { 
      assert($1 != nullptr);
      //std::cerr << "[UnaryExp] PrimaryExp ok\n";
      $$ = $1; 
    }
  | UnaryOp UnaryExp { 
      assert($1 != nullptr);
      assert($2 != nullptr);
      //std::cerr << "[UnaryExp] UnaryOp=" << *$1 << " UnaryExp ok\n";
      auto ast = new UnaryExpAST(); 
      ast->op = *$1; 
      ast->exp = std::unique_ptr<BaseAST>($2); 
      $$ = ast; 
    }
  ;

UnaryOp
  : '+' { 
    //std::cerr << "[UnaryOp] +\n"; 
  $$ = new std::string("+"); }
  | '-' { //std::cerr << "[UnaryOp] -\n"; 
  $$ = new std::string("-"); }
  | '!' { //std::cerr << "[UnaryOp] !\n"; 
  $$ = new std::string("!"); }
  ;

MulExp
  : UnaryExp { 
      assert($1 != nullptr);
      //std::cerr << "[MulExp] UnaryExp ok\n";
      $$ = $1; 
    }
  | MulExp '*' UnaryExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[MulExp] *\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "*"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | MulExp '/' UnaryExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[MulExp] /\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "/"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | MulExp '%' UnaryExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[MulExp] %\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "%"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  ;

AddExp
  : MulExp { 
      assert($1 != nullptr);
      //std::cerr << "[AddExp] MulExp ok\n";
      $$ = $1; 
    }
  | AddExp '+' MulExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[AddExp] +\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "+"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | AddExp '-' MulExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[AddExp] -\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "-"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  ;

RelExp
  : AddExp { 
      assert($1 != nullptr);
      //std::cerr << "[RelExp] AddExp ok\n";
      $$ = $1; 
    }
  | RelExp '<' AddExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[RelExp] <\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "<"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | RelExp '>' AddExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[RelExp] >\n";
      auto ast = new BinaryExpAST(); 
      ast->op = ">"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | RelExp LE AddExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[RelExp] <=\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "<="; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | RelExp GE AddExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[RelExp] >=\n";
      auto ast = new BinaryExpAST(); 
      ast->op = ">="; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  ;

EqExp
  : RelExp { 
      assert($1 != nullptr);
      //std::cerr << "[EqExp] RelExp ok\n";
      $$ = $1; 
    }
  | EqExp EQ RelExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[EqExp] ==\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "=="; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  | EqExp NE RelExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[EqExp] !=\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "!="; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  ;

LAndExp
  : EqExp { 
      assert($1 != nullptr);
      //std::cerr << "[LAndExp] EqExp ok\n";
      $$ = $1; 
    }
  | LAndExp AND EqExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      //std::cerr << "[LAndExp] &&\n";
      auto ast = new BinaryExpAST(); 
      ast->op = "&&"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  ;

LOrExp
  : LAndExp { 
      assert($1 != nullptr);
      $$ = $1; 
    }
  | LOrExp OR LAndExp { 
      assert($1 != nullptr);
      assert($3 != nullptr);
      auto ast = new BinaryExpAST(); 
      ast->op = "||"; 
      ast->lhs = std::unique_ptr<BaseAST>($1); 
      ast->rhs = std::unique_ptr<BaseAST>($3); 
      $$ = ast; 
    }
  ;

%%

void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}
