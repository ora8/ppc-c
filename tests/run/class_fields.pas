program class_fields;
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
  TCounter = class
  private
    class var FCreated: Integer;
  public
    class var Prefix: String;
    class var Default: optional TItem;
  var                                  { ends the class var section }
    Id: Integer;
    constructor Create;
    class function Created: Integer;
    function Text: String; const;
  end;
  TSub = class(TCounter)
    function Twice: Integer; const;
  end;
  TBox<T> = class
    class var Made: Integer;
    constructor Create;
  end;
  TConfig = record
    class var Verbose: Boolean;
    X: Integer;
  end;
constructor TItem.Create(AN: Integer); begin N := AN; end;
destructor TItem.Destroy; begin WriteLn('free item ', N); end;
constructor TCounter.Create;
begin
  Inc(FCreated);
  Id := FCreated;
end;
class function TCounter.Created: Integer; begin Result := FCreated; end;
function TCounter.Text: String; const; begin Result := Prefix + IntToStr(Id); end;
function TSub.Twice: Integer; const; begin Result := Created * 2; end;
constructor TBox<T>.Create; begin Inc(Made); end;
begin
  TCounter.Prefix := 'no ';
  vao a := TCounter.Create;
  vao b := TSub.Create;
  vao x := TBox<Integer>.Create;
  vao y := TBox<Integer>.Create;
  vao z := TBox<String>.Create;
  begin
    WriteLn(a.Text, ' ', b.Text, ' ', TCounter.Created, ' ', b.Twice);
    a.Prefix := '#';
    WriteLn(b.Text, ' ', TSub.Prefix);
    TCounter.Default := TItem.Create(7);
    if TCounter.Default is some d then WriteLn('default ', d.N);
    WriteLn(TBox<Integer>.Made, ' ', TBox<String>.Made);
    TConfig.Verbose := True;
    WriteLn(TConfig.Verbose);
  end;
  WriteLn('end');
end.
