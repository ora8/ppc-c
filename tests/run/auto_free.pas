program AutoFree;
{* objects and heap values in vao variables are freed automatically when their owner
  leaves its scope - unless the ownership was passed on. *}
type
  TNode = class
    Value: Integer;
    Next: optional TNode;            {* fields are freed with the object *}
    constructor Create(W: Integer);
    destructor Destroy; override;
  end;
  TList = class
    Head: optional TNode;
    procedure PushFront(W: Integer);
    procedure Show;
  end;

constructor TNode.Create(W: Integer); begin inherited Create; Value := W; end;
destructor TNode.Destroy; begin WriteLn('  freed: ', Value); inherited Destroy; end;

procedure TList.PushFront(W: Integer);
vao k := TNode.Create(W);
begin
  k.Next := Take(Head);
  Head := k;                           {* ownership moves into the list: k is not freed *}
end;

procedure TList.Show;
var p: optional TNode := none;
begin
  p := Head;
  while p is some k do begin Write(k.Value, ' '); p := k.Next; end;
  WriteLn;
end;

function Build(n: Integer): TList;     {* result: ownership goes to the caller *}
vao l := TList.Create;
begin
  for val i := 1 to n do l.PushFront(i);
  Result := l;
end;

procedure Fill(var Target: TNode);   {* returned through a var parameter *}
vao fresh := TNode.Create(77);
begin
  Target := fresh;                          {* the old Target is freed, fresh moves out *}
end;

procedure Local;
vao
  a := TNode.Create(1);
  I := 42;                              {* heap value *}
begin
  WriteLn('Local: a = ', a.Value, ', I = ', I);
end;                                    {* a and I are freed here *}

vao
  list: TList := Build(3);
  z := TNode.Create(5);

begin
  Local;
  WriteLn('-- list from a function');
  
  list.Show;
  WriteLn('-- overwriting frees the old object');
  list := Build(1);
  list.Show;
  WriteLn('-- var-Parameter');
  Fill(z);
  WriteLn('z = ', z.Value);
  WriteLn('-- temporary object');
  WriteLn(TNode.Create(9).Value);
  WriteLn('-- end of program');
end.
