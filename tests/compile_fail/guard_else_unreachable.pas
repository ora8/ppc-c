{ EXPECT: 'else' is unreachable: both 'ok' and 'error' are handled }
program T;
procedure P(r: result Integer);
begin
  case r of
    ok v when v > 1: WriteLn('big');
    ok v: WriteLn(v);
    error e: WriteLn(e);
  else WriteLn('?');
  end;
end;
begin P(ok(1)); end.
