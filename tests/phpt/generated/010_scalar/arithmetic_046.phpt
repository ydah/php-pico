--TEST--
integer arithmetic vector 046
--FILE--
<?php
$a = 29; $b = 2;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
31:27:58:1:60:116
