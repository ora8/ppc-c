{* EXPECT: the record "TSq" cannot be a value of interface "IShape" *}
program T; type IShape = interface function Area: Real; const; end; TSq = record(IShape) S: Real; function Area: Real; const; end; function TSq.Area: Real; const; begin Result := S; end; val q: TSq := (S: 1); begin var s: IShape := q; begin end; end.
