{ EXPECT: "Circle" has 1 field: Circle(Radius: Real) }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Circle(r, q): ; else end; end.
