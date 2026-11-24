program ObjectCreation;
{ Objekte entstehen nur im Initialisierer: 'vao x := TFoo.Create(...)' }
type
  TFoo = class
    V: Integer;
    constructor Create(AV: Integer);
    constructor Leer;
    destructor Destroy; override;
  end;
constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
constructor TFoo.Leer; begin inherited Create; V := -1; end;
destructor TFoo.Destroy; begin WriteLn('  Destroy ', V); inherited Destroy; end;

procedure P(n: Integer);
vao
  s := TFoo.Create(n);          { besitzt das Objekt }
  t := TFoo.Leer;               { anderer Konstruktor, ohne Klammern }
  h := TFoo.Create(n * 10);
  I: Integer := n + 1;          { besitzter Heapwert }
begin
  WriteLn('P: s.V = ', s.V, ', t.V = ', t.V, ', h.V = ', h.V, ', I = ', I);
  h.Free;
  I.Free;
end;

val
  a := 3;
begin
  P(2);
  a := 7;
  vao
    b := TFoo.Create(a);          { a hat hier den Wert 7 }
  begin
    a := 100;
    WriteLn('Block: b.V = ', b.V);
  end;
end.
