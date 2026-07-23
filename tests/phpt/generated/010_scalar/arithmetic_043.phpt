--TEST--
integer arithmetic vector 043
--FILE--
<?php
$a = 169; $b = 7;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
176:162:1183:1:345:1352
