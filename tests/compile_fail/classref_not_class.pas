{* EXPECT: 'class of' needs a class, "Integer" is not one *}
program T; type TShape = class Size: Integer; constructor Create(n: Integer); virtual; function Area: Integer; end; TShapeClass = class of TShape; constructor TShape.Create(n: Integer); begin Size := n; end; function TShape.Area: Integer; begin Result := Size; end;
type TR = class of Integer; begin end.
