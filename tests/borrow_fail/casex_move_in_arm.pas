{ EXPECT: use of possibly moved value 'k' }
program T;
type TShape = (Circle(Radius: Real), Empty);
  TK = class V: Integer; end;
function Eat(vao k: TK): Integer; begin Result := k.V; end;
procedure P(s: TShape);
vao k := TK.Create;
begin
  WriteLn(case s of Circle: Eat(k) else 0 end);
  WriteLn(k.V);
end;
begin P(Empty); end.
