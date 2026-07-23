--TEST--
integer arithmetic vector 061
--FILE--
<?php
$a = 93; $b = 23;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
116:70:2139:1:209:186
