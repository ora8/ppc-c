program DynamicArrays;
{ dynamic arrays (generics stage 5): array of T, SetLength, Length, High, Move, Clone }
type
  TK = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
    function Clone: TK; const; auto;
  end;
  TPoint = record X, Y: Integer; end;
  TStack<T> = class
    Items: array of optional T;
    procedure Push(vao x: T);
    function Pop: optional T;
    function Size: Integer; const;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  freed ', N); inherited Destroy; end;

procedure TStack<T>.Push(vao x: T);
begin
  SetLength(Items, Length(Items) + 1);
  Items[High(Items)] := x;
end;
function TStack<T>.Pop: optional T;
begin
  Result := Take(Items[High(Items)]);
  SetLength(Items, Length(Items) - 1);
end;
function TStack<T>.Size: Integer; const; begin Result := Length(Items); end;

function Sum(a: array of Integer): Integer;   { borrow, no copy }
begin
  Result := 0;
  for val i := 0 to High(a) do Result := Result + a[i];
end;

procedure Fill(var a: array of Integer; n: Integer);
begin
  SetLength(a, n);
  for val i := 0 to n - 1 do a[i] := i * i;
end;

val
  z: array of Integer := (1, 2, 3);
  p: array of TPoint := default;
  w: array of Integer := default;
vao
  objs: array of optional TK := default;
  st := TStack<TK>.Create;
  si := TStack<Integer>.Create;
begin
  WriteLn('Length ', Length(z), ' High ', High(z), ' Sum ', Sum(z));
  Fill(z, 5);
  WriteLn('after Fill: ', z[4], ' Sum ', Sum(z));
  SetLength(p, 2);
  p[1].X := 7;
  WriteLn('p[1].X = ', p[1].X, ', p[0].X = ', p[0].X);
  w := z;                                  { Move: z is empty afterwards }
  WriteLn('w ', Length(w), ', z ', Length(z));
  val c := w.Clone;                        { a copy of its own }
  begin
    c[0] := 99;
    WriteLn('w[0] = ', w[0], ', c[0] = ', c[0]);
  end;
  SetLength(objs, 3);
  objs[0] := TK.Create(1);
  objs[2] := TK.Create(3);
  SetLength(objs, 1);                      { frees TK 3 }
  WriteLn('objs ', Length(objs));
  st.Push(TK.Create(10));
  st.Push(TK.Create(20));
  if st.Pop is some k then WriteLn('pop ', k.N);
  for val i := 1 to 5 do si.Push(i * 10);
  WriteLn('si ', si.Size);
  WriteLn('End');
end.
