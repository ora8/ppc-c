{ EXPECT: this arm is never reached: an earlier arm already handles variant "Single" }
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Single(s): WriteLn(1); Single(Empty): WriteLn(2); else end; end; begin end.
