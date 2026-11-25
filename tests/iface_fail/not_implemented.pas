{ EXPECT: "TC" does not implement the interface "IShape" }
program T; type IShape = interface function Area: Real; const; end; TC = class function Area: Real; const; end; function TC.Area: Real; const; begin Result := 1 end; begin vao c := TC.Create; var s: IShape := c; begin end; end.
