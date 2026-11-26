{* EXPECT: interface "IShape" is listed twice *}
program T; type IShape = interface function Area: Real; const; end; TC = class(IShape, IShape) function Area: Real; const; end; function TC.Area: Real; const; begin Result := 1 end; begin end.
