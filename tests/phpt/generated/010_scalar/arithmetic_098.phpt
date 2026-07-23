--TEST--
integer arithmetic vector 098
--FILE--
<?php
$a = 149; $b = 15;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
164:134:2235:14:313:596
