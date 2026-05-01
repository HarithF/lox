try 
  begin
    match Sys.argv with
    [|_; outputdir|] ->
        let classes = 
          ["Assign - Token name, std::unique_ptr<Expr> expression";
          "Binary - std::unique_ptr<Expr> left, Token operator_, std::unique_ptr<Expr> right";
          "Call - std::unique_ptr<Expr> callee, Token paren, std::vector<std::unique_ptr<Expr>> args";
          "Get - std::unique_ptr<Expr> object, Token name";
          "Grouping - std::unique_ptr<Expr> expression";
          "Literal - LiteralValue value";
          "Logical - std::unique_ptr<Expr> left, Token operator_, std::unique_ptr<Expr> right";
          "Set - std::unique_ptr<Expr> object, Token name, std::unique_ptr<Expr> value";
          "Super - Token keyword, Token method";
          "This - Token keyword";
          "Unary - Token operator_, std::unique_ptr<Expr> right";
          "Ternary - std::unique_ptr<Expr> cond_, std::unique_ptr<Expr> then_b, std::unique_ptr<Expr> else_b";
          "Variable - Token name"] in 

        Define_ast.create_ast outputdir "Expr" classes "LiteralValue" ["token"; "<memory>"; "<vector>"];
        let stmts = 
          ["BlockStmt - std::vector<std::unique_ptr<Stmt>> statements";
          "ExprStmt - std::unique_ptr<Expr> expression";
          "FuncStmt - Token name, std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body";
          "ClassStmt - Token name, std::unique_ptr<Variable> superclass, std::vector<std::unique_ptr<FuncStmt>> methods";
          "IfStmt - std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> then_b, std::unique_ptr<Stmt> else_b";
          "PrintStmt - std::unique_ptr<Expr> expression";
          "ReturnStmt - Token keyword, std::unique_ptr<Expr> value";
          "VarStmt - Token name, std::unique_ptr<Expr> initializer";
          "WhileStmt - std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body";
          "BreakStmt - "] in 

        Define_ast.create_ast outputdir "Stmt" stmts "void" ["Expr"; "<vector>"]

    | _ -> 
        print_string "Usage: generate_ast <filename>";
        print_newline ()
end
with
  e -> 
    print_string "An error occurred: ";
    print_string (Printexc.to_string e);
    print_newline ();
    exit 1
