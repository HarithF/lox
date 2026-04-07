

let define_type ch super classinfo = 
  match String.split_on_char ':' classinfo with
| [classname; fields] ->
    let classname = String.trim classname in
    let fields = String.trim fields in
      output_string ch ("static class " ^ classname ^ " extends " ^ super ^ "{");
      output_char ch '\n';
      output_string ch fields;
      output_char ch '\n';
      output_char ch '}'

| _ -> raise (Invalid_argument "bad class format")
  

let define_ast ch super classes = 
  output_string ch ("abstract class " ^ super ^ " {");
  List.iter (define_type ch super) classes;

  output_char ch '}';
  output_char ch '\n'


let create_ast file super classes =
  let channel = open_out (file ^ super ^ ".cpp") in 
  define_ast channel super classes;
  close_out channel

