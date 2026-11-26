{* EXPECT: a generated Clone only reads Self: declare it 'function Clone: TA; const; auto;' *}
program T;
type TA = class function Clone: TA; auto; end;
begin end.
