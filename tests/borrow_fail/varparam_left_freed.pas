{* EXPECT: var parameter 'x' is left freed when 'Drop' returns *}
program T; type TFoo = class V: Integer; end;
procedure Drop(var x: TFoo); begin x.Free; end;
begin end.
