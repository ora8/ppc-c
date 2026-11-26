{* EXPECT: 'ClassName' is not known for type parameter "T": add a constraint that provides it (e.g. 'T: TBasis' for a class, 'T: Clone' for Clone) (in TBox<T>.Show) *}
program T;
type TBox<T> = class W: optional T; procedure Show; end;
procedure TBox<T>.Show; begin if W is some w then WriteLn(w.ClassName); end;
begin end.
