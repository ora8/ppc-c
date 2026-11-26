{* EXPECT: this arm is never reached: variant "Circle" is already handled by an arm without 'when' *}
program T;
type TShape = (Circle(Radius: Real), Empty);
procedure P(s: TShape);
begin
  case s of
    Circle(r): WriteLn('circle');
    Circle(r) when r > 1: WriteLn('big');
    Empty: WriteLn('empty');
  end;
end;
begin P(Empty); end.
