--TEST--
integer arithmetic vector 001
--FILE--
<?php
$a = 28; $b = 8;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
36:20:224:4:64:56
