{* EXPECT: a case expression on objects needs an 'else' (or an arm for "TFigure" itself) *}
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin WriteLn(case f of TCircle: 1; TSquare: 2 end); end; begin end.
