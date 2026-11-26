{* EXPECT: "TObject" is not an interface *}
program T; type IShape = interface function Area: Real; const; end; TC = class(TObject, TObject) end; begin end.
