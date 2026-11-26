{* EXPECT: case does not handle the variant Single of "TItem" completely *}
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Single(Circle(r)): WriteLn(r); Single(Empty): WriteLn(0); Pair(a, b): WriteLn(1); Box(c): WriteLn(2); end; end; begin end.
