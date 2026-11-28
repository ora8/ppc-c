program MethodValuesStored;
{* method values in local 'reference to' variables; closures that use objects and var parameters *}
type
  TCounter = class
    N: Integer;
    procedure Tick;
    procedure Run;
  end;
procedure TCounter.Tick;
begin
  Inc(N);
end;
procedure TCounter.Run;
val f: reference to procedure := Tick;       {* a method value of Self *}
begin
  f;
  f;
end;

procedure Twice(p: reference to procedure);
begin
  p;
  p;
end;

procedure AddTo(var total: Integer; amount: Integer);
begin
  Twice(procedure begin total := total + amount; end);   {* a closure that changes a var parameter *}
end;

procedure Demo;
vao a := TCounter.Create;
vao b := TCounter.Create;
val f: reference to procedure := a.Tick;
val total: Integer := 0;
begin
  f;                       {* a.Tick *}
  f := b.Tick;
  Twice(f);                {* b.Tick twice *}
  b.Run;                   {* b.Tick twice more *}
  Twice(procedure begin a.Tick; end);   {* a closure that uses an object *}
  WriteLn('a: ', a.N, ', b: ', b.N);
  AddTo(total, 5);
  WriteLn('total: ', total);
end;

begin
  Demo;
end.
