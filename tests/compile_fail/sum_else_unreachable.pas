{ EXPECT: 'else' is unreachable: all variants }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Circle(r): WriteLn(r); Rect, Empty: ; else end; end.
