--TEST--
UTC date formatting is deterministic
--FILE--
<?php
echo date('Y-m-d H:i:s D N w', 0);
--EXPECT--
1970-01-01 00:00:00 Thu 4 4
