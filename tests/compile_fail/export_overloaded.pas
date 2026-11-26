{* EXPECT: 'F' is overloaded: C has no overloading *}
library T; function F(a: Integer): Integer; overload; begin Result := a; end; function F(a: Real): Real; overload; begin Result := a; end; exports F; begin end.
