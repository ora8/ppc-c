{ EXPECT: cannot be compared with '=': it contains a function value or a result }
program T; type TR = record F: optional function: Integer; end; val a: TR := default; b: TR := default;
begin WriteLn(a = b); end.
