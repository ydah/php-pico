--TEST--
integer arithmetic vector 059
--FILE--
<?php
$a = 59; $b = 11;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
70:48:649:4:129:472
