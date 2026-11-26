{* EXPECT: the clone owns objects and would never be freed here: store it in a variable first ('val c := a.Clone') *}
program T;
type TX = class function Clone: TX; const; auto; end; TA = array[1..2] of optional TX;
procedure Show(z: TA); begin end;
val a: TA := default;
begin Show(a.Clone); end.
