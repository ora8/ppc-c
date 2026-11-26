{ EXPECT: this arm is never reached: "TCircle" objects are already handled by the arm for "TFigure" (put the more specific class first) }
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin case f of TFigure: WriteLn(1); TCircle: WriteLn(2); end; end; begin end.
