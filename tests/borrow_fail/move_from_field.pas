{ EXPECT: cannot move out of 'h.Next' (it is owned by its container); use Take(h.Next) }
program T; type TN = class Next: TN; end;
vao h: TN := TN.Create; n: TN := h.Next;
begin  h.Next := TN.Create;  n.Free; h.Free; end.
