{ EXPECT: is unreachable: an arm handles every "TFigure" }
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin case f of TCircle: WriteLn(1); TFigure: WriteLn(2); else WriteLn(3); end; end; begin end.
