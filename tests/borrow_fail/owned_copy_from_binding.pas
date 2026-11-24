{ EXPECT: 'p' owns its contents (objects or heap memory) and cannot be copied (assignment); move it from its owner or copy it with '.Clone' }
program T;
type TK = class end; TPaar = record L: optional TK; end;
procedure P; val o: optional TPaar := none; z: TPaar := default; begin if o is some p then z := p; end;
begin P; end.
