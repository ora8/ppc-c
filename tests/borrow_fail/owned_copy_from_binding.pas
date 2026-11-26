{* EXPECT: 'p' owns its contents (objects or heap memory) and cannot be copied (assignment); move it from its owner or copy it with '.Clone' *}
program T;
type TK = class end; TPair = record L: optional TK; end;
procedure P; val o: optional TPair := none; z: TPair := default; begin if o is some p then z := p; end;
begin P; end.
