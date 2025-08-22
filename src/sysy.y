%code requires
{
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
    int yylex();
    void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);
    extern int yylineno;
    using namespace std;
%}

%parse-param { std::unique_ptr<BaseAST> &ast }
%union
{
    std::string *str_val;
    int int_val;
    BaseAST *ast_val;
    std::vector<BaseAST*> *vec_blockitem;
    std::vector<ConstDeclAST::Def> *vec_constdef;
    std::vector<VarDeclAST::Def> *vec_vardef;
    ConstDeclAST::Def *constdef_ptr;
    VarDeclAST::Def *vardef_ptr;
}

%token INT RETURN CONST
%token <str_val> IDENT
%token <int_val> INT_CONST
%token LE GE EQ NE AND OR
%token IF ELSE WHILE BREAK CONTINUE

%type <ast_val> FuncDef FuncType Block BlockItem Stmt Decl ConstDecl VarDecl ConstInitVal InitVal Exp PrimaryExp Number UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp ConstExp
%type <ast_val> StmtNoBranch
%type <ast_val> IfStmtNoElse IfStmtWithElse WhileStmt
%type <str_val> UnaryOp LVal
%type <vec_blockitem> BlockItemList
%type <vec_constdef> ConstDefList
%type <vec_vardef> VarDefList
%type <constdef_ptr> ConstDef
%type <vardef_ptr> VarDef

%%

CompUnit
    : FuncDef
    {
        assert($1 != nullptr);
        auto comp_unit = make_unique<CompUnitAST>();
        comp_unit->func_def = unique_ptr<BaseAST>($1);
        ast = move(comp_unit);
    }
    ;

FuncDef
    : FuncType IDENT '(' ')' Block
    {
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
    : INT
    {
        auto ast = new FuncTypeAST();
        ast->type = "int";
        $$ = ast;
    }
    ;

Block
    : '{' '}'
    {
        auto ast = new BlockAST();
        $$ = ast;
    }
    | '{' BlockItemList '}'
    {
        auto ast = new BlockAST();
        for (auto &item : *static_cast<std::vector<BaseAST*>*>($2))
        {
            ast->items.push_back(unique_ptr<BaseAST>(item));
        }
        delete static_cast<std::vector<BaseAST*>*>($2);
        $$ = ast;
    }
    ;

BlockItemList
    : BlockItem
    {
        auto vec = new std::vector<BaseAST*>();
        vec->push_back($1);
        $$ = vec;
    }
    | BlockItemList BlockItem
    {
        auto vec = static_cast<std::vector<BaseAST*>*>($1);
        vec->push_back($2);
        $$ = vec;
    }
    ;

BlockItem
    : Decl
    {
        $$ = $1;
    }
    | Stmt
    {
        $$ = $1;
    }
    ;

Decl
    : ConstDecl
    {
        $$ = $1;
    }
    | VarDecl
    {
        $$ = $1;
    }
    ;

ConstDecl
    : CONST INT ConstDefList ';'
    {
        auto ast = new ConstDeclAST();
        for (auto &def : *static_cast<std::vector<ConstDeclAST::Def>*>($3))
        {
            ast->defs.push_back(std::move(def));
        }
        delete static_cast<std::vector<ConstDeclAST::Def>*>($3);
        $$ = ast;
    }
    ;

ConstDefList
    : ConstDef
    {
        auto vec = new std::vector<ConstDeclAST::Def>();
        vec->push_back(std::move(*$1));
        delete $1;
        $$ = vec;
    }
    | ConstDefList ',' ConstDef
    {
        auto vec = $1;
        vec->push_back(std::move(*$3));
        delete $3;
        $$ = vec;
    }
    ;

ConstDef
    : IDENT '=' ConstInitVal
    {
        auto def = new ConstDeclAST::Def();
        def->name = *unique_ptr<string>($1);
        def->val = unique_ptr<BaseAST>($3);
        $$ = def;
    }
    ;

ConstInitVal
    : ConstExp
    {
        $$ = $1;
    }
    ;

ConstExp
    : Exp
    {
        $$ = $1;
    }
    ;

VarDecl
    : INT VarDefList ';'
    {
        auto ast = new VarDeclAST();
        for (auto &def : *static_cast<std::vector<VarDeclAST::Def>*>($2))
        {
            ast->defs.push_back(std::move(def));
        }
        delete static_cast<std::vector<VarDeclAST::Def>*>($2);
        $$ = ast;
    }
    ;

VarDefList
    : VarDef
    {
        auto vec = new std::vector<VarDeclAST::Def>();
        vec->push_back(std::move(*$1));
        delete $1;
        $$ = vec;
    }
    | VarDefList ',' VarDef
    {
        auto vec = $1;
        vec->push_back(std::move(*$3));
        delete $3;
        $$ = vec;
    }
    ;

VarDef
    : IDENT
    {
        auto def = new VarDeclAST::Def();
        def->name = *unique_ptr<string>($1);
        def->has_init = false;
        $$ = def;
    }
    | IDENT '=' InitVal
    {
        auto def = new VarDeclAST::Def();
        def->name = *unique_ptr<string>($1);
        def->val = unique_ptr<BaseAST>($3);
        def->has_init = true;
        $$ = def;
    }
    ;

InitVal
    : Exp
    {
        $$ = $1;
    }
    ;

/* ==== 拆分法核心 ==== */
Stmt
    : IfStmtWithElse
    | IfStmtNoElse
    | StmtNoBranch
    | WhileStmt
    | BREAK ';'
      {
        auto ast = new StmtAST();
        ast->kind = StmtAST::BREAK_STMT;
        $$ = ast;
      }
    | CONTINUE ';'
      {
        auto ast = new StmtAST();
        ast->kind = StmtAST::CONTINUE_STMT;
        $$ = ast;
      }
    ;

IfStmtNoElse
    : IF '(' Exp ')' Stmt
    {
        auto ast = new IfStmtAST();
        ast->cond = std::unique_ptr<BaseAST>($3);
        ast->then_stmt = std::unique_ptr<BaseAST>($5);
        ast->has_else = false;
        $$ = ast;
    }
    ;

IfStmtWithElse
    : IF '(' Exp ')' IfStmtWithElse ELSE Stmt
    {
        auto ast = new IfStmtAST();
        ast->cond = std::unique_ptr<BaseAST>($3);
        ast->then_stmt = std::unique_ptr<BaseAST>($5);
        ast->else_stmt = std::unique_ptr<BaseAST>($7);
        ast->has_else = true;
        $$ = ast;
    }
    | IF '(' Exp ')' StmtNoBranch ELSE Stmt
    {
        auto ast = new IfStmtAST();
        ast->cond = std::unique_ptr<BaseAST>($3);
        ast->then_stmt = std::unique_ptr<BaseAST>($5);
        ast->else_stmt = std::unique_ptr<BaseAST>($7);
        ast->has_else = true;
        $$ = ast;
    }
    ;

StmtNoBranch
    : LVal '=' Exp ';'
    {
        auto ast = new StmtAST();
        ast->kind = StmtAST::ASSIGN;
        ast->lval = *unique_ptr<string>($1);
        ast->exp = unique_ptr<BaseAST>($3);
        $$ = ast;
    }
    | Exp ';'
    {
        auto ast = new StmtAST();
        ast->kind = StmtAST::EXPR;
        ast->has_exp = true;
        ast->exp = unique_ptr<BaseAST>($1);
        $$ = ast;
    }
    | ';'
    {
        auto ast = new StmtAST();
        ast->kind = StmtAST::EXPR;
        ast->has_exp = false;
        $$ = ast;
    }
    | Block
    {
        auto ast = new StmtAST();
        ast->kind = StmtAST::BLOCK;
        ast->block = unique_ptr<BaseAST>($1);
        $$ = ast;
    }
    | RETURN Exp ';'
    {
        auto ast = new StmtAST();
        ast->kind = StmtAST::RETURN;
        ast->has_exp = true;
        ast->exp = unique_ptr<BaseAST>($2);
        $$ = ast;
    }
    | RETURN ';'
    {
        auto ast = new StmtAST();
        ast->kind = StmtAST::RETURN;
        ast->has_exp = false;
        $$ = ast;
    }
    ;

WhileStmt
    : WHILE '(' Exp ')' Stmt
    {
        auto while_ast = new WhileStmtAST();
        while_ast->cond = std::unique_ptr<BaseAST>($3);
        while_ast->body = std::unique_ptr<BaseAST>($5);
        auto stmt = new StmtAST();
        stmt->kind = StmtAST::WHILE_STMT;
        stmt->while_stmt = std::unique_ptr<BaseAST>(while_ast);
        $$ = stmt;
    }
    ;

LVal
    : IDENT
    {
        $$ = $1;
    }
    ;

Exp
    : LOrExp
    {
        auto ast = new ExpAST();
        ast->lor_exp = std::unique_ptr<BaseAST>($1);
        $$ = ast;
    }
    ;

LAndExp
  : EqExp {
      $$ = $1;
    }
  | LAndExp AND EqExp {
      auto ast = new BinaryExpAST();
      ast->op = "&&";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  ;

LOrExp
  : LAndExp {
      $$ = $1;
    }
  | LOrExp OR LAndExp {
      auto ast = new BinaryExpAST();
      ast->op = "||";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  ;

EqExp
  : RelExp {
      $$ = $1;
    }
  | EqExp EQ RelExp {
      auto ast = new BinaryExpAST();
      ast->op = "==";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  | EqExp NE RelExp {
      auto ast = new BinaryExpAST();
      ast->op = "!=";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  ;

RelExp
  : AddExp {
      $$ = $1;
    }
  | RelExp '<' AddExp {
      auto ast = new BinaryExpAST();
      ast->op = "<";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  | RelExp '>' AddExp {
      auto ast = new BinaryExpAST();
      ast->op = ">";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  | RelExp LE AddExp {
      auto ast = new BinaryExpAST();
      ast->op = "<=";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  | RelExp GE AddExp {
      auto ast = new BinaryExpAST();
      ast->op = ">=";
      ast->lhs = std::unique_ptr<BaseAST>($1);
      ast->rhs = std::unique_ptr<BaseAST>($3);
      $$ = ast;
    }
  ;

AddExp
    : MulExp
    {
        $$ = $1;
    }
    | AddExp '+' MulExp
    {
        auto ast = new BinaryExpAST();
        ast->op = "+";
        ast->lhs = std::unique_ptr<BaseAST>($1);
        ast->rhs = std::unique_ptr<BaseAST>($3);
        $$ = ast;
    }
    | AddExp '-' MulExp
    {
        auto ast = new BinaryExpAST();
        ast->op = "-";
        ast->lhs = std::unique_ptr<BaseAST>($1);
        ast->rhs = std::unique_ptr<BaseAST>($3);
        $$ = ast;
    }
    ;

MulExp
    : UnaryExp
    {
        $$ = $1;
    }
    | MulExp '*' UnaryExp
    {
        auto ast = new BinaryExpAST();
        ast->op = "*";
        ast->lhs = std::unique_ptr<BaseAST>($1);
        ast->rhs = std::unique_ptr<BaseAST>($3);
        $$ = ast;
    }
    | MulExp '/' UnaryExp
    {
        auto ast = new BinaryExpAST();
        ast->op = "/";
        ast->lhs = std::unique_ptr<BaseAST>($1);
        ast->rhs = std::unique_ptr<BaseAST>($3);
        $$ = ast;
    }
    | MulExp '%' UnaryExp
    {
        auto ast = new BinaryExpAST();
        ast->op = "%";
        ast->lhs = std::unique_ptr<BaseAST>($1);
        ast->rhs = std::unique_ptr<BaseAST>($3);
        $$ = ast;
    }
    ;

UnaryExp
  : PrimaryExp {
      $$ = $1;
    }
  | UnaryOp UnaryExp {
      auto ast = new UnaryExpAST();
      ast->op = *$1;
      ast->exp = std::unique_ptr<BaseAST>($2);
      $$ = ast;
    }
  ;

UnaryOp
    : '+'
    {
        $$ = new std::string("+");
    }
    | '-'
    {
        $$ = new std::string("-");
    }
    | '!'
    {
        $$ = new std::string("!");
    }
    ;

PrimaryExp
    : '(' Exp ')'
    {
        auto ast = new PrimaryExpAST();
        ast->is_number = false;
        ast->exp = std::unique_ptr<BaseAST>($2);
        $$ = ast;
    }
    | LVal
    {
        auto ast = new IdentAST();
        ast->name = *unique_ptr<string>($1);
        $$ = ast;
    }
    | Number
    {
        auto ast = new PrimaryExpAST();
        ast->is_number = true;
        ast->number_value = dynamic_cast<NumberAST*>($1)->value;
        $$ = ast;
    }
    ;

Number
    : INT_CONST
    {
        auto ast = new NumberAST();
        ast->value = $1;
        $$ = ast;
    }
    ;

%%

void yyerror(std::unique_ptr<BaseAST> &ast, const char *s)
{
    std::cerr << "error: " << s << " at line " << yylineno << std::endl;
}