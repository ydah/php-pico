--TEST--
integer arithmetic vector 048
--FILE--
<?php
$a = 63; $b = 14;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
77:49:882:7:140:63
