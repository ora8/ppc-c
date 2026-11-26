{ EXPECT: an arm with names or with some/ok/error can only have one label }
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Single(Circle(r)), Single(Empty): WriteLn(1); else end; end; begin end.
