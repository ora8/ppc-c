{* EXPECT: "optional TFigure" may be none: unpack it first *}
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: optional TFigure); begin if f is TCircle(c) then WriteLn(c.R); end; begin end.
