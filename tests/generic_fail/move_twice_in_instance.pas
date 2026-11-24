{ EXPECT: use of moved value 'x' (ownership was transferred away) }
program T;
type TK = class end;
  TBox<T> = class A, B: optional T; procedure MoveTwice(vao x: T); end;
procedure TBox<T>.MoveTwice(vao x: T); begin A := x; B := x; end;
vao i := TBox<Integer>.Create;
vao k := TBox<TK>.Create;
begin end.
