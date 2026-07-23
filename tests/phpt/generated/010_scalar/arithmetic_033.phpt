--TEST--
integer arithmetic vector 033
--FILE--
<?php
$a = 190; $b = 16;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
206:174:3040:14:396:380
