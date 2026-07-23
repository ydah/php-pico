--TEST--
integer arithmetic vector 035
--FILE--
<?php
$a = 33; $b = 5;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
38:28:165:3:71:264
