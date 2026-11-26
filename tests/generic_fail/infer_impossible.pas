{* EXPECT: cannot infer type parameter 'T' of "Fresh" from the arguments: write 'Fresh<...>(...)' *}
program T;
function Fresh<T>: Integer; begin Result := 0; end;
begin WriteLn(Fresh); end.
