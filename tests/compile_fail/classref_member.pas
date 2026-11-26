{* EXPECT: through a class reference only constructors, class methods and ClassName can be used *}
program T; type TShape = class Size: Integer; constructor Create(n: Integer); virtual; function Area: Integer; end; TShapeClass = class of TShape; constructor TShape.Create(n: Integer); begin Size := n; end; function TShape.Area: Integer; begin Result := Size; end;
procedure P(c: TShapeClass); begin WriteLn(c.Area); end; begin end.
