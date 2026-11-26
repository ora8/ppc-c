{* EXPECT: 'a' owns its contents (objects or heap memory) and cannot be copied (initializer); move it from its owner or copy it with '.Clone' *}
program T;
procedure P(a: array of Integer); val b: array of Integer := a; begin end;
begin end.
