{* EXPECT: "TC" does not implement the method "Area" of interface "IShape" *}
program T; type IShape = interface function Area: Real; const; end; TC = class(IShape) end; begin end.
