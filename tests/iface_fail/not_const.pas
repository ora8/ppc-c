{* EXPECT: must be declared 'const' like "Area" of interface "IShape" *}
program T; type IShape = interface function Area: Real; const; end; TC = class(IShape) function Area: Real; end; function TC.Area: Real; begin Result := 1 end; begin end.
