--TEST--
integer arithmetic vector 060
--FILE--
<?php
$a = 76; $b = 17;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
93:59:1292:8:169:76
