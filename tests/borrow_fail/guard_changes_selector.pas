{* EXPECT: the value of this 'case' must not change while its arm is chosen or its names are in use *}
program T;
type TShape = (Circle(Radius: Real), Empty);
function Reset(var s: TShape): Boolean; begin s := Empty; Result := True; end;
procedure P(var s: TShape);
begin
  case s of
    Circle(r) when Reset(s): WriteLn(r);
  else WriteLn('other');
  end;
end;
begin end.
