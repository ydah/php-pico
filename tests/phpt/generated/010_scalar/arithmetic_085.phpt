--TEST--
integer arithmetic vector 085
--FILE--
<?php
$a = 119; $b = 6;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
125:113:714:5:244:238
