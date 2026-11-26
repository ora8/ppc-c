program StackMove;
{* val b := k: a stack object is moved (copied byte for byte), k is dead afterwards *}
type
  TK = class
    N: Integer;
    Part: optional TK;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  Destroy ', N); inherited Destroy; end;

procedure P(flag: Boolean);
val k := TK.Create(1);
begin
  k.Part := TK.Create(2);
  if flag then
  begin
    val b := k;                       {* k moves to b *}
    begin
      b.N := 10;
      WriteLn('b = ', b.N);
    end;                              {* b (with Part) is destroyed here *}
  end;
  WriteLn('P end');
end;                                  {* k only if it was not moved *}

begin
  P(True);
  P(False);
end.
