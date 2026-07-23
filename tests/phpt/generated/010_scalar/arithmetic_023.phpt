--TEST--
integer arithmetic vector 023
--FILE--
<?php
$a = 20; $b = 2;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
22:18:40:0:42:160
