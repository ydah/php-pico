--TEST--
integer arithmetic vector 030
--FILE--
<?php
$a = 139; $b = 21;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
160:118:2919:13:299:556
