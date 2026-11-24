{ EXPECT: 'w' owns objects and cannot be copied (assignment); only its owner can move it away }
program T;
type TK = class end; TPaar = record L: optional TK; end;
procedure P; val q: TPaar := default; val w: TPaar const := q; val z: TPaar := default; begin z := w; end;
begin P; end.
