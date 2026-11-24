{ EXPECT: 'auto' belongs to the declaration in the class or record ('function Clone: T; const; auto;') }
program T;
type TA = class function Clone: TA; const; end; function TA.Clone: TA; const; auto; begin Result := TA.Create; end;
begin end.
