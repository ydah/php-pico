--TEST--
integer arithmetic vector 069
--FILE--
<?php
$a = 38; $b = 2;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
40:36:76:0:78:76
