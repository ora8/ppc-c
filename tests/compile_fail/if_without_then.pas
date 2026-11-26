{* EXPECT: 'then' expected but identifier 'WriteLn' found *}
program T;
val n: Integer := 1;
begin
  if n < 2 WriteLn(n);
end.
