{* EXPECT: "TB" cannot be cloned: its field 'G' owns a "TX", which has no Clone (declare 'function Clone: TX; const; auto;' there) *}
program T;
type TX = class end; TA = class function Clone: TA; const; virtual; auto; end; TB = class(TA) G: optional TX; end;
begin end.
