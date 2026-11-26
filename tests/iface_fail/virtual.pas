{* EXPECT: interface methods are plain headers *}
program T; type IShape = interface function Area: Real; const; end; IBad = interface procedure P; virtual; end; begin end.
