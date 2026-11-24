{ EXPECT: "array[1..2] of optional TX" cannot be cloned: each element owns a "TX", which has no Clone (declare 'function Clone: TX; const; auto;' there) }
program T;
type TX = class end; TA = array[1..2] of optional TX;
val a: TA := default; b: TA := default;
begin b := a.Clone; end.
