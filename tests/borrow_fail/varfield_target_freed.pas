{ EXPECT: 'b' is used after the object it refers to through a 'var' field is gone: 'a' was freed }
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
begin vao a := TNode.Create('a'); vao b := TNode.Create('b'); begin b.Parent := a; a.Free; WriteLn(b.Name); end; end.
