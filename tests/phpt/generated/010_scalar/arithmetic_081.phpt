--TEST--
integer arithmetic vector 081
--FILE--
<?php
$a = 51; $b = 5;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
56:46:255:1:107:102
