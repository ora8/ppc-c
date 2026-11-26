{* EXPECT: a constant in a pattern needs a number, character or string, not "TShape" *}
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Single(5): WriteLn(1); else end; end; begin end.
