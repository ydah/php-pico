--TEST--
integer arithmetic vector 047
--FILE--
<?php
$a = 46; $b = 8;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
54:38:368:6:100:368
