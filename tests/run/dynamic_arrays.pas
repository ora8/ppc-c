program DynamicArrays;
{ Dynamische Arrays (Generics Stufe 5): array of T, SetLength, Length, High, Move, Clone }
type
  TK = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
    function Clone: TK; const; auto;
  end;
  TPunkt = record X, Y: Integer; end;
  TStapel<T> = class
    Elemente: array of optional T;
    procedure Push(vao x: T);
    function Pop: optional T;
    function Anzahl: Integer; const;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  frei ', N); inherited Destroy; end;

procedure TStapel<T>.Push(vao x: T);
begin
  SetLength(Elemente, Length(Elemente) + 1);
  Elemente[High(Elemente)] := x;
end;
function TStapel<T>.Pop: optional T;
begin
  Result := Take(Elemente[High(Elemente)]);
  SetLength(Elemente, Length(Elemente) - 1);
end;
function TStapel<T>.Anzahl: Integer; const; begin Result := Length(Elemente); end;

function Summe(a: array of Integer): Integer;   { Ausleihe, keine Kopie }
begin
  Result := 0;
  for val i := 0 to High(a) do Result := Result + a[i];
end;

procedure Fuelle(var a: array of Integer; n: Integer);
begin
  SetLength(a, n);
  for val i := 0 to n - 1 do a[i] := i * i;
end;

val
  z: array of Integer := (1, 2, 3);
  p: array of TPunkt := default;
  w: array of Integer := default;
vao
  objs: array of optional TK := default;
  st := TStapel<TK>.Create;
  si := TStapel<Integer>.Create;
begin
  WriteLn('Length ', Length(z), ' High ', High(z), ' Summe ', Summe(z));
  Fuelle(z, 5);
  WriteLn('nach Fuelle: ', z[4], ' Summe ', Summe(z));
  SetLength(p, 2);
  p[1].X := 7;
  WriteLn('p[1].X = ', p[1].X, ', p[0].X = ', p[0].X);
  w := z;                                  { Move: z ist danach leer }
  WriteLn('w ', Length(w), ', z ', Length(z));
  val c := w.Clone;                        { eigene Kopie }
  begin
    c[0] := 99;
    WriteLn('w[0] = ', w[0], ', c[0] = ', c[0]);
  end;
  SetLength(objs, 3);
  objs[0] := TK.Create(1);
  objs[2] := TK.Create(3);
  SetLength(objs, 1);                      { gibt TK 3 frei }
  WriteLn('objs ', Length(objs));
  st.Push(TK.Create(10));
  st.Push(TK.Create(20));
  if st.Pop is some k then WriteLn('pop ', k.N);
  for val i := 1 to 5 do si.Push(i * 10);
  WriteLn('si ', si.Anzahl);
  WriteLn('Ende');
end.
