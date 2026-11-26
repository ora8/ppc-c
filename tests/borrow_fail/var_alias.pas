{* EXPECT: cannot borrow 'x' as mutable (var) more than once *}
program T;
procedure Swap(var a, b: Integer); val t: Integer := default; begin t := a; a := b; b := t; end;
val x: Integer := default;
begin x := 1; Swap(x, x); end.
