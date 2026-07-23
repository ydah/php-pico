--TEST--
integer arithmetic vector 092
--FILE--
<?php
$a = 47; $b = 2;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
49:45:94:1:96:47
