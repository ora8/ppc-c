{* EXPECT: a type pattern with a name ('is TCircle(x)') can only be the whole condition of an 'if' (or use 'case') *}
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end; TSquare = class(TFigure) S: Integer; end; TOther = class end;
procedure P(f: TFigure); begin if (f.X > 0) and (f is TCircle(c)) then WriteLn(1); end; begin end.
