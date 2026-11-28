{* EXPECT: a thread class cannot have the 'var' field 'Other': the thread would use an object of the program at the same time (give it the object itself) *}
program T;
import std;
type TK = class end; TW = class(TThread) Other: var TK; procedure Execute; override; end;
procedure TW.Execute; begin end;
begin end.
