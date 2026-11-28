program CaseObjects;
{* case expressions that give objects: every arm makes a new one (or is none); classes give their common ancestor *}
type
  TFigure = class
    function Name: String; virtual; const;
    destructor Destroy; override;
  end;
  TCircle = class(TFigure) function Name: String; override; const; end;
  TSquare = class(TFigure) function Name: String; override; const; end;
function TFigure.Name: String; const; begin Result := 'figure'; end;
destructor TFigure.Destroy; begin WriteLn('  freed ', Name); inherited Destroy; end;
function TCircle.Name: String; const; begin Result := 'circle'; end;
function TSquare.Name: String; const; begin Result := 'square'; end;

function Make(k: Integer): TFigure;
begin
  Result := case k of 1: TCircle.Create; 2: TSquare.Create else TFigure.Create end;
end;

function Maybe(k: Integer): optional TFigure;
begin
  Result := case k of 1: TCircle.Create else none end;
end;

begin
  for val k := 1 to 3 do
    vao f := Make(k);
    begin
      WriteLn(k, ': ', f.Name);
    end;
  vao g := case 2 of 1: TCircle.Create else TSquare.Create end;
  begin
    WriteLn('direct: ', g.Name);
  end;
  if Maybe(1) is some m then WriteLn('maybe: ', m.Name);
  if Maybe(2) is some m then WriteLn('?') else WriteLn('maybe: none');
  WriteLn((case 1 of 1: TCircle.Create else TSquare.Create end).Name);
end.
