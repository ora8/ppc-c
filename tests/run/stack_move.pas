program StackMove;
{ val b := k: ein Stackobjekt wird verschoben (byteweise kopiert), k ist danach tot }
type
  TK = class
    N: Integer;
    Teil: optional TK;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  Destroy ', N); inherited Destroy; end;

procedure P(flag: Boolean);
val k := TK.Create(1);
begin
  k.Teil := TK.Create(2);
  if flag then
  begin
    val b := k;                       { k wandert nach b }
    begin
      b.N := 10;
      WriteLn('b = ', b.N);
    end;                              { b (mit Teil) wird hier abgebaut }
  end;
  WriteLn('P Ende');
end;                                  { k nur, wenn es nicht verschoben wurde }

begin
  P(True);
  P(False);
end.
