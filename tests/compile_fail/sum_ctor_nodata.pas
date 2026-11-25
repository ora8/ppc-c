{ EXPECT: "Empty" has no data }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin s := Empty(1); end.
