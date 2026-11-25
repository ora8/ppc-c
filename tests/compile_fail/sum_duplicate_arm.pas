{ EXPECT: variant "Circle" is handled twice }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Circle, Circle: ; else end; end.
