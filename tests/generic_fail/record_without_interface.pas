{* EXPECT: "TSq" does not satisfy 'T: IShape' *}
program T; type IShape = interface function Area: Real; const; end; TSq = record S: Real; function Area: Real; const; end; function TSq.Area: Real; const; begin Result := S; end; function A<T: IShape>(x: T): Real; begin Result := x.Area; end; val q: TSq := (S: 1); begin WriteLn(A(q)); end.
