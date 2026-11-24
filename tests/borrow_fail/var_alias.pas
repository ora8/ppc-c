{ EXPECT: cannot borrow 'x' as mutable (var) more than once }
program T;
procedure Swap(var a, b: Integer); val t: Integer; begin t := a; a := b; b := t; end;
val x: Integer;
begin x := 1; Swap(x, x); end.
