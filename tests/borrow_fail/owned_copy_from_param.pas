{ EXPECT: 'p' owns its contents (objects or heap memory) and cannot be copied (initializer); move it from its owner or copy it with '.Clone' }
program T;
type TK = class end; TPaar = record L: optional TK; end;
procedure P(p: TPaar); val c: TPaar := p; begin end;
begin end.
