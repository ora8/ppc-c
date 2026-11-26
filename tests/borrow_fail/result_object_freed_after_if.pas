{* EXPECT: 'f' was freed at the end of the 'if' at line 20 *}
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
procedure P; var g: optional TFile := none; begin if Open('x') is ok f then g := f; if g is some q then q.Write('x'); end; begin end.
