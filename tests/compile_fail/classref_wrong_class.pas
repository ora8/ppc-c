{* EXPECT: got "class of TOther" expected "class of TShape" *}
program T; type TShape = class Size: Integer; constructor Create(n: Integer); virtual; function Area: Integer; end; TShapeClass = class of TShape; constructor TShape.Create(n: Integer); begin Size := n; end; function TShape.Area: Integer; begin Result := Size; end;
type TOther = class end; val c: TShapeClass := TOther; begin end.
