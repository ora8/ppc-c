{* EXPECT: the name 'r' is used twice in the pattern *}
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Pair(Circle(r), Circle(r)): WriteLn(r); else end; end; begin end.
