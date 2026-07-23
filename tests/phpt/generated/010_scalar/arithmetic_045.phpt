--TEST--
integer arithmetic vector 045
--FILE--
<?php
$a = 12; $b = 19;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
31:-7:228:12:43:24
