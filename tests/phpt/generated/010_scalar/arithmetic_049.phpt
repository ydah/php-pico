--TEST--
integer arithmetic vector 049
--FILE--
<?php
$a = 80; $b = 20;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
100:60:1600:0:180:160
