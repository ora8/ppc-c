{* EXPECT: the 'when' condition must be Boolean, got "Real" *}
program T;
type TShape = (Circle(Radius: Real), Empty);
procedure P(s: TShape);
begin
  case s of
    Circle(r) when r: WriteLn('big');
  else WriteLn('other');
  end;
end;
begin P(Empty); end.
