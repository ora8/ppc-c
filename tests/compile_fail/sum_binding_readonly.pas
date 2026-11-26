{* EXPECT: cannot modify constant/loop variable "r" *}
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Circle(r): r := 2; else end; end.
