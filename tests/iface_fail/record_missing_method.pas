{* EXPECT: "TSq" does not implement the method "Area" of interface "IShape" *}
program T; type IShape = interface function Area: Real; const; end; TSq = record(IShape) S: Real; end; begin end.
