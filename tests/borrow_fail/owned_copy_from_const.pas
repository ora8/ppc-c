{ EXPECT: 'w' owns its contents (objects or heap memory) and cannot be copied (assignment); move it from its owner or copy it with '.Clone' }
program T;
type TK = class end; TPaar = record L: optional TK; end;
procedure P; val q: TPaar := default; val w: TPaar const := q; val z: TPaar := default; begin z := w; end;
begin P; end.
