--TEST--
integer arithmetic vector 041
--FILE--
<?php
$a = 135; $b = 18;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
153:117:2430:9:288:270
