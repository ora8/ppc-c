{ EXPECT: assignment: cannot modify a part of 'q', which is constant }
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin case f of TSquare(q): q.S := 1; end; end; begin end.
