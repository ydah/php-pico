--TEST--
integer arithmetic vector 093
--FILE--
<?php
$a = 64; $b = 8;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
72:56:512:0:136:128
