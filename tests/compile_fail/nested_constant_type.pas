{* EXPECT: incompatible types in comparison: "Integer" and "Char" *}
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Single(Rect('a', h)): WriteLn(h); else end; end; begin end.
