{ EXPECT: 'p' owns objects and cannot be copied (assignment); only its owner can move it away }
program T;
type TK = class end; TPaar = record L: optional TK; end;
procedure P; val o: optional TPaar := none; z: TPaar := default; begin if o is some p then z := p; end;
begin P; end.
