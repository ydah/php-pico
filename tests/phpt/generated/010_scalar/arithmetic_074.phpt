--TEST--
integer arithmetic vector 074
--FILE--
<?php
$a = 123; $b = 9;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
132:114:1107:6:255:492
