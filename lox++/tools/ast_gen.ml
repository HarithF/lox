try 
  begin
    match Sys.argv with
    [|_; outputdir|] ->
        let classes = 
          ["Binary - std::unique_ptr<Expr> left, Token operator_, std::unique_ptr<Expr> right";
          "Grouping - std::unique_ptr<Expr> expression";
          "Literal - LiteralValue value";
          "Unary - Token operator_, std::unique_ptr<Expr> right";
          "Ternary - std::unique_ptr<Expr> cond_, std::unique_ptr<Expr> then_b, std::unique_ptr<Expr> else_b"] in 

        Define_ast.create_ast outputdir "Expr" classes

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
