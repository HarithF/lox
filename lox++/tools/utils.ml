let contains_substring str sub =
  let len_str = String.length str in
  let len_sub = String.length sub in
  if len_sub > len_str then false
  else
    let rec check i =
      if i > len_str - len_sub then false
      else if String.sub str i len_sub = sub then true
      else check (i + 1)
    in
    check 0
