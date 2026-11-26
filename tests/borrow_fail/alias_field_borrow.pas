{* EXPECT: borrowed reference 'k' is used after its owner 'a.Child' was reassigned at line 3 *}
program T; type TK = class Value: Integer; Child: optional TK; procedure Increment; function Info: Integer; const; end; procedure TK.Increment; begin Inc(Value); end; function TK.Info: Integer; const; begin Result := Value; end;
procedure P; vao a := TK.Create; var k: optional TK const := a.Child; begin a.Child := TK.Create;  a.Child := none; if k is some x then WriteLn(x.Info); end;
begin end.
