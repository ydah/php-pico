--TEST--
integer arithmetic vector 028
--FILE--
<?php
$a = 105; $b = 9;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
114:96:945:6:219:105
