{ EXPECT: borrowed reference 'r' is used here, but 'it' was changed at line 7 while 'r' was borrowing it }
program T;
type TShape = (Circle(Radius: Real), Empty); TItem = (Single(S: TShape), Nothing);
procedure P(var it: TItem);
begin
  case it of
    Single(Circle(r)): begin it := Nothing; WriteLn(r); end;
  else
  end;
end;
begin end.
