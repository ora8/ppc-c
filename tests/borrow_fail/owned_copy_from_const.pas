{ EXPECT: 'w' owns its contents (objects or heap memory) and cannot be copied (assignment); move it from its owner or copy it with '.Clone' }
program T;
type TK = class end; TPair = record L: optional TK; end;
procedure P; val q: TPair := default; val w: TPair const := q; val z: TPair := default; begin z := w; end;
begin P; end.
