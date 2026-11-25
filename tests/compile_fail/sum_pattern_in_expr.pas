{ EXPECT: can only be the whole condition of an 'if' }
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin WriteLn((s is Circle(r)) and True); end.
