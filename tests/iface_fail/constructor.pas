{* EXPECT: interfaces have no constructors or destructors *}
program T; type IShape = interface function Area: Real; const; end; IBad = interface constructor Create; end; begin end.
