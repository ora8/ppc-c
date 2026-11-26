{* EXPECT: a value that owns objects cannot be copied into a literal; build it in place *}
program T;
type TK = class end; TPair = record L: optional TK; end;
type TA = array[1..2] of TPair; procedure P; val q: TPair := default; a: TA := default; begin a := (q, q); end;
begin P; end.
