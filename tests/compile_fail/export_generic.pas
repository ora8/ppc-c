{* EXPECT: the generic routine 'Id' cannot be exported *}
library T; function Id<T>(x: T): T; begin Result := x; end; exports Id; begin end.
