let newline ch = 
  output_char ch '\n'

let define_base_struct ch super ret_type= 
  output_string ch ("struct " ^ super ^ " {\n");
  output_string ch ("\tvirtual ~" ^ super ^ "() = default;\n");
  output_string ch ("\tvirtual " ^ ret_type ^ " accept(" ^ super ^ "Visitor&) = 0;\n");
  output_string ch ("};\n\n")

let define_constructor ch classname fields=
  if String.trim fields = "" then
        output_string ch ("\t" ^ classname ^ "() {}\n")
  else
    let inits =
      List.map (fun fld ->
        match String.split_on_char ' ' (String.trim fld) with
        | [typ; name] ->
            if Utils.contains_substring typ "std::unique_ptr" || typ = "LiteralValue"
            then name ^ "(std::move(" ^ name ^ "))"
            else name ^ "(" ^ name ^ ")"
        | _ -> raise (Invalid_argument "bad sub-class")
      ) (String.split_on_char ',' fields)
    in

  output_string ch ("\t" ^ classname ^ "(");
  output_string ch fields;
  output_string ch ")\n\t\t: ";
  output_string ch (String.concat ", " inits);
  output_string ch " {}\n"

let define_visitor ch super classes ret_type=
    let names = List.map (fun cls ->
        match String.split_on_char '-' cls with
        | [name; _] -> String.trim name
        | _ -> ""
    ) classes in
    output_string ch ("struct " ^ super ^ "Visitor {\n");
    List.iter (fun name ->
        output_string ch ("\tvirtual " ^ ret_type ^ " visit(" ^ name ^ "&) = 0;\n")
    ) names;
    output_string ch ("\tvirtual ~" ^ super ^ "Visitor() = default;\n};\n\n")

let define_forward_decls ch classes =
    List.iter (fun cls ->
        match String.split_on_char '-' cls with
        | [name; _] ->
            output_string ch ("struct " ^ String.trim name ^ ";\n")
        | _ -> ()
    ) classes;
    output_char ch '\n'


let define_type ch super ret_type classinfo = 
  match String.split_on_char '-' classinfo with
| [classname; fields] ->
    let classname = String.trim classname in
    let fields = String.trim fields in
      output_string ch ("struct " ^ classname ^ " : public  " ^ super ^ " {");
      newline ch;
      if fields <> "" then begin
      List.iter (fun fld ->
          match String.split_on_char ' ' (String.trim fld) with
          [typ ; name] -> 
            output_string ch ("\t" ^ typ ^ " " ^ name ^ ";\n");
              newline ch 
          | _ -> raise (Invalid_argument "bad sub-class")
        )
        (String.split_on_char ',' fields);
        newline ch;
      end;
      define_constructor ch classname fields;
      output_string ch ("\t" ^ ret_type ^ " accept(" ^ super ^ "Visitor& visitor) override {\n");
      if ret_type = "void" then
        output_string ch ("\t\tvisitor.visit(*this);\n")
      else
        output_string ch ("\t\treturn visitor.visit(*this);\n");
      output_string ch ("\t}\n");
      output_string ch "};\n";


| _ -> raise (Invalid_argument "bad class format")
  

let define_ast ch super classes ret_type pckgs= 
  output_string ch ("#pragma once\n");
  List.iter (fun inc ->
    if String.length inc > 0 && inc.[0] = '<' then
        output_string ch ("#include " ^ inc ^ "\n")
    else
        output_string ch ("#include \"" ^ inc ^ ".h\"\n")
) pckgs;
  newline ch;
  define_forward_decls ch classes;
  define_visitor ch super classes ret_type;
  define_base_struct ch super ret_type;
  List.iter (define_type ch super ret_type) classes;
  newline ch


let create_ast file super classes ret_type pckg=
  let channel = open_out (file ^ super ^ ".h") in 
  define_ast channel super classes ret_type pckg;
  close_out channel

