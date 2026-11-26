program OwnedValues;
{* arrays and records that own objects: they are moved instead of copied and
  freed with all their objects at the end of their scope *}
type
  TK = class N: Integer; constructor Create(AN: Integer); destructor Destroy; override; end;
  TPair = record
    Name: String;
    L, R: optional TK;
  end;
  TList = array[1..3] of TPair;
  THolder = class
    P: TPair;
    A: array[1..2] of optional TK;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  freed ', N); inherited Destroy; end;

function Make(n: Integer): TPair;
begin
  Result.Name := 'p' + IntToStr(n);
  Result.L := TK.Create(n);
end;

procedure Show(p: TPair);              {* borrow, no copy *}
begin
  if p.L is some k then WriteLn('  ', p.Name, ': ', k.N);
end;

procedure Local;
val
  a: array[1..2] of optional TK := default;
  q: TPair := default;
  z: TPair := default;
vao
  b: array[1..2] of optional TK := default;
  h := THolder.Create;
begin
  a[1] := TK.Create(1);
  a[2] := TK.Create(2);
  b[1] := TK.Create(3);
  q := Make(4);
  Show(q);
  z := q;                               {* Move: q is empty afterwards *}
  WriteLn('  q empty: ', q.L = none);
  q := Make(5);
  q := Make(6);                         {* the old q (5) is freed *}
  h.P := Make(7);
  h.A[2] := TK.Create(8);
  Make(9);                              {* unused: freed at once *}
  WriteLn('end of Local');
end;

val l: TList := default;
begin
  Local;
  l[1] := Make(10);
  l[2].R := TK.Create(11);
  WriteLn('end of program');
end.
