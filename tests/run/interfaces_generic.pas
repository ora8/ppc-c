program interfaces_generic;
type
  ILess<T> = interface
    function Less(a, b: T): Boolean; const;
  end;
  TByLength = class(ILess<String>)
    function Less(a, b: String): Boolean; const;
  end;
  TDown = class(ILess<Integer>)
    function Less(a, b: Integer): Boolean; const;
  end;
function TByLength.Less(a, b: String): Boolean; const; begin Result := Length(a) < Length(b); end;
function TDown.Less(a, b: Integer): Boolean; const; begin Result := a > b; end;
procedure SortWith<T: Copy>(var a: array of T; c: ILess<T> const);
begin
  for val i := 1 to High(a) do
    val j := i;
    begin
      while (j > 0) and c.Less(a[j], a[j - 1]) do
      begin
        val h: T := a[j];
        begin a[j] := a[j - 1]; a[j - 1] := h; end;
        Dec(j);
      end;
    end;
end;
val
  w: array of String := ('ccc', 'a', 'bb');
  n: array of Integer := (1, 3, 2);
begin
  vao bl := TByLength.Create;
  vao d := TDown.Create;
  begin
    SortWith(w, bl);
    SortWith(n, d);
  end;
  for val x in w do Write(x, ' ');
  for val x in n do Write(x, ' ');
  WriteLn;
end.
