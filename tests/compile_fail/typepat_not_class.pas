{* EXPECT: "Integer" is not a class (a case on "TFigure" tests the class of the object) *}
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin case f of Integer: WriteLn(1); end; end; begin end.
