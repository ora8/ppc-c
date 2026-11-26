{* EXPECT: "TOther" is not an interface (a record lists the interfaces it implements: record(IShape)) *}
program T; type TOther = class end; TSq = record(TOther) S: Real; end; begin end.
