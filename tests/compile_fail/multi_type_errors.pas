{* EXPECT: 7 error(s), compilation aborted *}
program E2;
type
  TFoo = class
    V: Integer;
    procedure Show;
  end;
procedure TFoo.Show; begin WriteLn(V + 'x'); end;

function F(n: Integer): Integer;
val s: String := 5;
val value := 3;
begin
  Result := n * 2;
  Result := 'text';
  Unknown(3);
  WriteLn(value + 1);
end;

val i := 1;
begin
  i := 'abc';
  WriteLn(F(1, 2));
  if i then WriteLn('x');
  WriteLn(i);
end.
