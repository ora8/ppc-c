{ EXPECT: 'p' owns objects and cannot be copied (initializer); only its owner can move it away }
program T;
type TK = class end; TPaar = record L: optional TK; end;
procedure P(p: TPaar); val c: TPaar := p; begin end;
begin end.
