{ EXPECT: 'b.Parent' would refer to 'a', which is freed before 'b' (declare 'a' before 'b') }
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
begin vao b := TNode.Create('b'); begin begin vao a := TNode.Create('a'); begin b.Parent := a; end; end; WriteLn(b.Name); end; end.
