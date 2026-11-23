{ EXPECT: cannot borrow 'x' as mutable (var) more than once }
program T;
procedure Swap(var a, b: Integer); var t: Integer; begin t := a; a := b; b := t; end;
var x: Integer;
begin x := 1; Swap(x, x); end.
