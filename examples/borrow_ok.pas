program BorrowOk;
{ Ownership rules of ppc with a small address book.
  Owners:    'vao' variables, fields of class type
  Borrows:   'var' variables, parameters (also 'T const')
  Move:      assignment owner -> owner, 'vao' parameters, Take()
  optional:  only such references may be missing (none), e.g. the end of a list;
             they are used after unpacking with 'is some' }
type
  TPerson = class
    Name: String;
    Next: optional TPerson;               { owns the successor, none at the end }
    constructor Create(AName: String const);
    destructor Destroy; override;
  end;

  TBook = class
    FirstPerson: optional TPerson;              { owns the whole chain }
    procedure Fresh(vao P: TPerson);     { takes over the ownership of P }
    function Find(N: String const): var optional TPerson;  { only borrows }
    function TakeOut: optional TPerson;   { gives the ownership to the caller }
    destructor Destroy; override;
  end;

constructor TPerson.Create(AName: String const);
begin inherited Create; Name := AName; end;

destructor TPerson.Destroy;
begin inherited Destroy; end;                 { Next is freed automatically }

procedure TBook.Fresh(vao P: TPerson);
begin
  P.Next := Take(FirstPerson);                 { FirstPerson becomes none, the chain moves to P.Next }
  FirstPerson := P;                            { Move: P is used up afterwards }
end;

function TBook.Find(N: String const): var optional TPerson;
var p: optional TPerson := none;
begin
  Result := none;
  p := FirstPerson;
  while p is some x do begin
    if x.Name = N then begin Result := x; Exit; end;
    p := x.Next;
  end;
end;

function TBook.TakeOut: optional TPerson;
begin
  Result := Take(FirstPerson);
  if Result is some r then FirstPerson := Take(r.Next);
end;

destructor TBook.Destroy;
begin inherited Destroy; end;                 { FirstPerson (and the chain) automatically }

procedure Show(B: TBook);               { value parameter = borrow, must not free }
var p: optional TPerson := none;
begin
  p := B.FirstPerson;
  while p is some x do begin Write(x.Name, ' '); p := x.Next; end;
  WriteLn;
end;

vao
  book := TBook.Create;                  { owners }
  anna: TPerson := TPerson.Create('Anna');
  gone: optional TPerson := none;
var
  hit: optional TPerson := none;     { borrow }
begin
  
  book.Fresh(TPerson.Create('Bert'));      { pass a temporary object directly }
  
  book.Fresh(anna);                        { Move: anna owns nothing any more }
  anna := TPerson.Create('Carla');       { assigning again is allowed }
  book.Fresh(anna);
  Show(book);

  hit := book.Find('Anna');         { borrow from the book }
  if hit is some t then WriteLn('found: ', t.Name);

  gone := book.TakeOut;                   { ownership comes back }
  if gone is some w then WriteLn('taken: ', w.Name);
  Show(book);
end.                                     { book, anna and gone are freed here }
