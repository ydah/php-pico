--TEST--
integer arithmetic vector 055
--FILE--
<?php
$a = 182; $b = 10;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
192:172:1820:2:374:1456
