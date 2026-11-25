program text_files;
import ppc_std;
begin
  case TTextFile.OpenWrite('tf_test.tmp') of
    ok f: begin f.WriteLine('first'); f.Write('sec'); f.WriteLine('ond'); end;
    error e: WriteLn(e);
  end;
  if TTextFile.OpenAppend('tf_test.tmp') is ok f then f.WriteLine('third');
  if TTextFile.OpenRead('tf_test.tmp') is ok f then
    loop
      begin
        if f.ReadLine is some line then WriteLn(f.Name, ': ', line) else Break;
      end;
  if TTextFile.OpenRead('/no/such/file') is error e then WriteLn(e);
  if DeleteFile('tf_test.tmp') is ok then WriteLn('removed');
end.
