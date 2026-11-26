{* EXPECT: is an enumeration with data *}
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin WriteLn(s.Radius); end.
