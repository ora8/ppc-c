{ EXPECT: an interface contains only method headers }
program T; type IShape = interface function Area: Real; const; end; IBad = interface X: Integer; end; begin end.
