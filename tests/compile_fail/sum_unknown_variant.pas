{ EXPECT: "Square" is not a variant of "TShape" }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Square: ; else end; end.
