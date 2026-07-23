--TEST--
integer arithmetic vector 082
--FILE--
<?php
$a = 68; $b = 11;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
79:57:748:2:147:272
