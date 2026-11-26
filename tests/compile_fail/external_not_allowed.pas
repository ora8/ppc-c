{* EXPECT: external routines are not supported *}
program T; function F(x: Real): Real; external 'sin'; begin end.
