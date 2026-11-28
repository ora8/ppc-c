program Threads;
{* threads: a TThread object moves into Start and comes back with Join; the thread uses only its own
   object (no global variables, no 'var' fields), so threads never share data *}
import std;
type
  TSum = class(TThread)
    From, Upto: Int64;
    Total: Int64;
    Name: String;
    Seen: TList<Int64>;
    constructor Create(AFrom, AUpto: Int64; AName: String);
    procedure Execute; override;
    destructor Destroy; override;
  end;
function Square(x: Int64): Int64;
begin
  Result := x * x;
end;
constructor TSum.Create(AFrom, AUpto: Int64; AName: String);
begin
  inherited Create;
  From := AFrom;
  Upto := AUpto;
  Name := AName;
  Seen := TList<Int64>.Create;
end;
procedure TSum.Execute;
begin
  Total := 0;
  for val i := From to Upto do
  begin
    Total := Total + Square(i);
    if i mod 250000 = 0 then Seen.Add(i);
  end;
  Name := Name + ' done';
end;
destructor TSum.Destroy;
begin
  Seen.Free;
  inherited Destroy;
end;
vao a := TSum.Create(1, 500000, 'first');
vao b := TSum.Create(500001, 1000000, 'second');
vao c := TSum.Create(1, 10, 'never joined');
begin
  WriteLn('cpus: ', CPUCount > 0);
  vao ra := Start(a);
  vao rb := Start(b);
  vao rc := Start(c);
  begin
    vao da := ra.Join;
    vao db := rb.Join;
    begin
      WriteLn(da.Name, ': ', da.Total, ' ', da.Seen.Count);
      WriteLn(db.Name, ': ', db.Total, ' ', db.Seen.Count);
      WriteLn('sum: ', da.Total + db.Total);
    end;
  end;
end.
