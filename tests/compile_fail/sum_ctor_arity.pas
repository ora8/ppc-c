{* EXPECT: "Circle" needs 1 value: Circle(Radius: Real) *}
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin s := Circle(1, 2); end.
