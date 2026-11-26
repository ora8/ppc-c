{* EXPECT: 'a-b' is not a valid C name *}
program T; procedure F; external 'a-b'; begin end.
