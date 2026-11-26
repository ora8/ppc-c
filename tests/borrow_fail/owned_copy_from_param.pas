{* EXPECT: 'p' owns its contents (objects or heap memory) and cannot be copied (initializer); move it from its owner or copy it with '.Clone' *}
program T;
type TK = class end; TPair = record L: optional TK; end;
procedure P(p: TPair); val c: TPair := p; begin end;
begin end.
