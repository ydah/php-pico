--TEST--
integer arithmetic vector 040
--FILE--
<?php
$a = 118; $b = 12;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
130:106:1416:10:248:118
