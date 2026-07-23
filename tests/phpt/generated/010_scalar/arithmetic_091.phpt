--TEST--
integer arithmetic vector 091
--FILE--
<?php
$a = 30; $b = 19;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
49:11:570:11:79:240
