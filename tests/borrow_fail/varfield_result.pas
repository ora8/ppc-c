{* EXPECT: cannot store the reference to 'a' in the 'var' field 'Result.Parent' *}
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
function Mk: TNode; vao a := TNode.Create('a'); begin Result := TNode.Create('r'); Result.Parent := a; end; begin end.
