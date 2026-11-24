program ReadonlyBorrows;
{ read-only object borrows: 'T const' (like &T in Rust, const T& in C++) }
type
  TPerson = class
    Name: String;
    Age: Integer;
    Friend: optional TPerson;               { owns the Friend }
    constructor Create(AName: String const; AAge: Integer);
    procedure Birthday;                   { changes the object }
    function Info: String; const;           { only reads }
    function BestFriend: var optional TPerson const; const;
  end;

constructor TPerson.Create(AName: String const; AAge: Integer);
begin inherited Create; Name := AName; Age := AAge; end;
procedure TPerson.Birthday; begin Inc(Age); end;
function TPerson.Info: String; const; begin Result := Name + ' (' + IntToStr(Age) + ')'; end;
function TPerson.BestFriend: var optional TPerson const; const;
begin Result := Friend; end;                { Friend is only readable here }

procedure Show(P: TPerson const);          { const parameter: the object is read-only }
begin
  Write(P.Info);
  if P.BestFriend is some f then Write(', Friend: ', f.Info);
  WriteLn;
end;

procedure Celebrate(P: TPerson);               { normal borrow: may change it }
begin P.Birthday; end;

function Older(A, B: TPerson const): var TPerson const;
begin
  if A.Age >= B.Age then Result := A else Result := B;
end;

vao
  anna := TPerson.Create('Anna', 30);
  bert := TPerson.Create('Bert', 28);
begin
  anna.Friend := TPerson.Create('Carla', 31);
  Show(anna);
  Celebrate(bert);
  Celebrate(bert);
  Show(bert);
  var reader: TPerson const := anna;   { read-only borrow (changeable -> read-only) }
  begin
    WriteLn('reader: ', reader.Info);
    WriteLn('older: ', Older(anna, bert).Info);
    var k: TPerson const := bert;                         { const reference: the object is read-only }
    begin
      WriteLn('k: ', k.Info);
    end;
    WriteLn('reader last: ', reader.Info); { last use of reader }
  end;
  anna.Birthday;                          { allowed: reader is not used any more afterwards }
  WriteLn('anna after Birthday: ', anna.Info);
end.
