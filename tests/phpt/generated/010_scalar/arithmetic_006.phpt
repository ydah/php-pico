--TEST--
integer arithmetic vector 006
--FILE--
<?php
$a = 113; $b = 15;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
128:98:1695:8:241:452
