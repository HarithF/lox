let newline ch = 
  output_char ch '\n'


let define_constructor ch classname fields =
    output_string ch ("\t" ^ classname ^ "(");
    output_string ch fields;
    output_string ch ")\n\t\t: ";
    List.iter (fun fld ->
        match String.split_on_char ' ' (String.trim fld) with
        [typ ; name] -> 
            if typ = "std::unique_ptr<Expr>" 
              then  output_string ch (name ^ "(std::move(" ^ name ^ ")), ") 
              else output_string ch (name ^ "(" ^ name ^ "), ")
        | _ -> raise (Invalid_argument "bad sub-class")
      )
      (String.split_on_char ',' fields);

    output_string ch " {}\n"

let define_type ch super classinfo = 
  match String.split_on_char '-' classinfo with
| [classname; fields] ->
    let classname = String.trim classname in
    let fields = String.trim fields in
      output_string ch ("struct " ^ classname ^ " : public  " ^ super ^ " {");
      newline ch;
     List.iter (fun fld ->
        match String.split_on_char ' ' (String.trim fld) with
        [typ ; name] -> 
          output_string ch ("\t" ^ typ ^ " " ^ name ^ ";\n");
            newline ch 
        | _ -> raise (Invalid_argument "bad sub-class")
      )
      (String.split_on_char ',' fields);
      newline ch;
      define_constructor ch classname fields;
      output_string ch "};\n"

| _ -> raise (Invalid_argument "bad class format")
  

let define_ast ch super classes = 
  output_string ch ("#pragma once\n#include \"token.h\"\n#include <memory>\n\n");
output_string ch ("struct " ^ super ^ " {\n\tvirtual ~" ^ super ^ "() = default;\n};\n\n");
  List.iter (define_type ch super) classes;

  newline ch


let create_ast file super classes =
  let channel = open_out (file ^ super ^ ".cpp") in 
  define_ast channel super classes;
  close_out channel

