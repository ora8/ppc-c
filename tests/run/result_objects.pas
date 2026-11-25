program result_objects;
type
  TFile = class
    Name: String;
    Lines: Integer;
    constructor Create(AName: String);
    destructor Destroy; override;
    procedure Write(s: String);
  end;
constructor TFile.Create(AName: String); begin Name := AName; end;
destructor TFile.Destroy; begin WriteLn('close ', Name, ' (', Lines, ' lines)'); end;
procedure TFile.Write(s: String); begin Inc(Lines); end;

function Open(Name: String): result TFile;
begin
  if Name = '' then Result := error('no name')
  else Result := ok(TFile.Create(Name));
end;

function OpenLog(Name: String): result TFile;
begin
  vao f := try Open(Name + '.log');
  begin
    f.Write('header');
    Result := ok(f);
  end;
end;

function Count(Names: array of String): result Integer;
begin
  Result := ok(0);
  for val n in Names do
  begin
    vao f := try Open(n);
    begin
      f.Write('x');
    end;
    Result := ok(Result.ValueOr(0) + 1);
  end;
end;

val
  names: array of String := ('a', '', 'b');
  r := Open('stored');
begin
  if Open('a.txt') is ok f then f.Write('hello');
  if Open('') is ok f then WriteLn('no') else WriteLn('failed');
  if Open('') is error e then WriteLn('error: ', e);
  if Open('freed.txt') is error e then WriteLn(e) else WriteLn('ok, not taken');
  if r is ok f then f.Write('stored line');
  case Open('case.txt') of
    ok f: f.Write('in case');
    error e: WriteLn(e);
  end;
  if OpenLog('app') is ok f then WriteLn(f.Name, ' ', f.Lines);
  if OpenLog('') is error e then WriteLn('log: ', e);
  for val i := 1 to 2 do
    if Open('loop' + IntToStr(i)) is ok f then f.Write('l');
  if Count(names) is error e then WriteLn('count: ', e);
  r := Open('second');
  WriteLn('end');
end.
