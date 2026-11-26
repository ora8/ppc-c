{* EXPECT: cannot store borrowed reference 'x' in owning variable 'k' *}
program T; type TFoo = class V: Integer; end;
procedure P(x: TFoo); vao k: TFoo := x; begin  end;
begin end.
