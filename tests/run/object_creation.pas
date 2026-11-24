program ObjectCreation;
{ objects are only created in the initializer: 'vao x := TFoo.Create(...)' }
type
  TFoo = class
    V: Integer;
    constructor Create(AV: Integer);
    constructor Empty;
    destructor Destroy; override;
  end;
constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
constructor TFoo.Empty; begin inherited Create; V := -1; end;
destructor TFoo.Destroy; begin WriteLn('  Destroy ', V); inherited Destroy; end;

procedure P(n: Integer);
vao
  s := TFoo.Create(n);          { owns the object }
  t := TFoo.Empty;               { another constructor, without parentheses }
  h := TFoo.Create(n * 10);
  I: Integer := n + 1;          { owned heap value }
begin
  WriteLn('P: s.V = ', s.V, ', t.V = ', t.V, ', h.V = ', h.V, ', I = ', I);
end;                            { s, t, h and I are freed here }

val
  a := 3;
begin
  P(2);
  a := 7;
  vao
    b := TFoo.Create(a);          { a has the value 7 here }
  begin
    a := 100;
    WriteLn('Block: b.V = ', b.V);
  end;
end.
