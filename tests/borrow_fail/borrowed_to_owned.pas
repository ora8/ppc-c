{ EXPECT: cannot pass borrowed reference 'x' to owned parameter 'o' of 'Keep' }
program T; type TFoo = class V: Integer; end;
var Store: TFoo;
procedure Keep(owned o: TFoo); begin Store := o; end;
procedure P(x: TFoo); begin Keep(x); end;
begin end.
