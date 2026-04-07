try 
  begin
    match Sys.argv with
    [|_; outputdir|] ->
        let classes = 
          ["Binary : Expr left, Token operator, Expr right";
          "Grouping : Expr expression";
          "Literal : Object value";
          "Unary : Token operator, Expr right"] in 

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
