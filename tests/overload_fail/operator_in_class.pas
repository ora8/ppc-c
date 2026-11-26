{* EXPECT: operators can only be declared in records *}
program T;
type TA = class class operator Add(a, b: TA): TA; end;
begin end.
