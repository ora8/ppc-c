{ EXPECT: cannot move out of 'h.Next' (it is owned by its container); use Take(h.Next) }
program T; type TN = class Next: TN; end;
var h, n: TN;
begin h := TN.Create; h.Next := TN.Create; n := h.Next; n.Free; h.Free; end.
