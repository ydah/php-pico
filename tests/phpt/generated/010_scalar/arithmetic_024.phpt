--TEST--
integer arithmetic vector 024
--FILE--
<?php
$a = 37; $b = 8;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
45:29:296:5:82:37
