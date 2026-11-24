{ EXPECT: "TA" cannot be cloned: its field 'F' owns a "TX", which has no Clone (declare 'function Clone: TX; const; auto;' there) }
program T;
type TX = class end; TA = class F: optional TX; function Clone: TA; const; auto; end;
begin end.
