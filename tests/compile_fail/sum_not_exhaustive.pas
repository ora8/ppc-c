{ EXPECT: case does not handle the variants Rect, Empty of "TShape" }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Circle(r): WriteLn(r); end; end.
