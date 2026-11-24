{ EXPECT: a value that owns objects cannot be copied into a literal; build it in place }
program T;
type TK = class end; TPaar = record L: optional TK; end;
type TA = array[1..2] of TPaar; procedure P; val q: TPaar := default; a: TA := default; begin a := (q, q); end;
begin P; end.
