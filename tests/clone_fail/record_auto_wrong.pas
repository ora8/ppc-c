{ EXPECT: Clone of record "TR" must return "TR" }
program T;
type TR = record X: Integer; function Clone: Integer; const; auto; end;
begin end.
