{ EXPECT: was resized by SetLength }
program T;
function Get<T>(var a: array of optional T): var optional T; begin Result := a[0]; end; procedure Use2<T>(x: T); begin end; procedure P<T>(var a: array of optional T); begin if Get(a) is some r then begin SetLength(a, 0); Use2(r); end; end;
begin end.
