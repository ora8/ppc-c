{ EXPECT: identifier not found "k" }
program T;
type TShape = (Circle(Radius: Real), Empty);
procedure P(s: TShape);
begin
  case s of
    Circle(r) when k > 1: WriteLn('big');
  else WriteLn('other');
  end;
end;
begin P(Empty); end.
