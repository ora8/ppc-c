{* EXPECT: constraints belong to the declaration of the type, not to its methods: 'procedure TBox<...>.P' *}
program T;
type TBox<T: Comparable> = class procedure P; end; procedure TBox<T: Comparable>.P; begin end;
begin end.
