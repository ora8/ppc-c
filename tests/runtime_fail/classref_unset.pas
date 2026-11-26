{* EXPECT: the class reference is not set *}
program T; type TShape = class Size: Integer; constructor Create(n: Integer); virtual; function Area: Integer; end; TShapeClass = class of TShape; constructor TShape.Create(n: Integer); begin Size := n; end; function TShape.Area: Integer; begin Result := Size; end;
val a: array of TShapeClass := default; begin SetLength(a, 1); vao s := a[0].Create(1); begin end; end.
