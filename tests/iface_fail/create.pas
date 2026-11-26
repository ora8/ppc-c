{* EXPECT: "IShape" is an interface: create an object of a class *}
program T; type IShape = interface function Area: Real; const; end; begin vao s := IShape.Create; begin end; end.
