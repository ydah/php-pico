--TEST--
integer arithmetic vector 031
--FILE--
<?php
$a = 156; $b = 4;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
160:152:624:0:316:1248
