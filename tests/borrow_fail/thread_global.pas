{* EXPECT: TW.Execute runs in its own thread and must not use the global variable 'count': threads do not share variables (give the data to the thread object) *}
program T;
import std;
type TW = class(TThread) N: Integer; procedure Execute; override; end;
val count: Integer := 0;
procedure TW.Execute; begin Inc(count); end;
begin end.
