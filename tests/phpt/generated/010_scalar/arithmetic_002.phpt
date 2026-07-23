--TEST--
integer arithmetic vector 002
--FILE--
<?php
$a = 45; $b = 14;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
59:31:630:3:104:180
