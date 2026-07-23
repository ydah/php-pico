--TEST--
integer arithmetic vector 096
--FILE--
<?php
$a = 115; $b = 3;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
118:112:345:1:233:115
