{ EXPECT: 'try' on a stored result would take its object }
program r2;
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
function G: result TFile; val r := Open('s'); begin vao f := try r; begin Result := ok(f); end; end; begin end.
