{* EXPECT: this arm is never reached: 'some' is already handled by an arm without 'when' *}
program T;
procedure P(o: optional Integer);
begin
  case o of
    some v: WriteLn(v);
    some v when v > 1: WriteLn('big');
    none: WriteLn('none');
  end;
end;
begin P(none); end.
