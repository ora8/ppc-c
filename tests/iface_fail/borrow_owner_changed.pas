{ EXPECT: borrowed reference 's' is used after its owner 'c' was reassigned }
program T; type IShape = interface function Area: Real; const; end; TC = class(IShape) function Area: Real; const; end; function TC.Area: Real; const; begin Result := 1 end; begin vao c := TC.Create; var s: IShape := c; begin c := TC.Create; WriteLn(s.Area); end; end.
