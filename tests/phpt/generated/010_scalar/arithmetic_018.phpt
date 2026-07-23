--TEST--
integer arithmetic vector 018
--FILE--
<?php
$a = 126; $b = 18;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
144:108:2268:0:270:504
