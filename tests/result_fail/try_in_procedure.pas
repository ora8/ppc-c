{* EXPECT: 'try' passes the error to the caller, so it needs a function that returns a result ('function F: result T') *}
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
procedure P; begin val x := try T2(1); begin WriteLn(x); end; end;
begin P; end.
