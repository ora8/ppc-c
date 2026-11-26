{* EXPECT: cannot pass borrowed reference 'x' to owned parameter 'o' of 'Keep' *}
program T; type TFoo = class V: Integer; end;
vao Store: optional TFoo := none;
procedure Keep(vao o: TFoo); begin Store := o; end;
procedure P(x: TFoo); begin Keep(x); end;
begin end.
