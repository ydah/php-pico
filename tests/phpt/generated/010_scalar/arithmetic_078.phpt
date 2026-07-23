--TEST--
integer arithmetic vector 078
--FILE--
<?php
$a = 191; $b = 10;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
201:181:1910:1:392:764
