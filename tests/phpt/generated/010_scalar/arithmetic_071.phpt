--TEST--
integer arithmetic vector 071
--FILE--
<?php
$a = 72; $b = 14;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
86:58:1008:2:158:576
