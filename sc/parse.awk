BEGIN { FS="\n"; print "start transaction;"; }
{print "insert into dev_info values (\"VST"$1"\"," e ", 30, 33002, 1);"}
END {print "commit;"}
