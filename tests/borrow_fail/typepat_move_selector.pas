{* EXPECT: borrowed reference 'c' is used after its owner 'f' was moved into parameter 'f' of 'Eat' *}
program T;
type TFigure = class X: Integer; end; TCircle = class(TFigure) R: Real; end;
procedure Eat(vao f: TFigure); begin end;
procedure P;
vao f: TFigure := TCircle.Create;
begin
  case f of
    TCircle(c): begin Eat(f); WriteLn(c.R); end;
  else
  end;
end;
begin P; end.
