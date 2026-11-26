{ EXPECT: case does not handle the variant Circle of "TShape" (add it or an 'else') }
program T;
type TShape = (Circle(Radius: Real), Empty);
procedure P(s: TShape);
begin
  case s of
    Circle(r) when r > 1: WriteLn('big');
    Empty: WriteLn('empty');
  end;
end;
begin P(Empty); end.
