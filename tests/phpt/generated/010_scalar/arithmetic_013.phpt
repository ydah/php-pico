--TEST--
integer arithmetic vector 013
--FILE--
<?php
$a = 41; $b = 11;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
52:30:451:8:93:82
