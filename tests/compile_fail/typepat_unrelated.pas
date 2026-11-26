{* EXPECT: "TOther" is not a descendant of "TFigure": this arm can never match *}
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin case f of TOther: WriteLn(1); end; end; begin end.
