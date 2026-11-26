{* EXPECT: an object made through a class reference gets its class only at run time *}
program T; type TShape = class Size: Integer; constructor Create(n: Integer); virtual; function Area: Integer; end; TShapeClass = class of TShape; constructor TShape.Create(n: Integer); begin Size := n; end; function TShape.Area: Integer; begin Result := Size; end;
procedure P(c: TShapeClass); val s := c.Create(1); begin end; begin end.
