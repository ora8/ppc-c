{ EXPECT: "TC.Area" has a different signature than "Area" of interface "IShape" }
program T; type IShape = interface function Area: Real; const; end; TC = class(IShape) function Area: Integer; const; end; function TC.Area: Integer; const; begin Result := 1 end; begin end.
