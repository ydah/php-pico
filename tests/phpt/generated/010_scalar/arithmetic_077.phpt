--TEST--
integer arithmetic vector 077
--FILE--
<?php
$a = 174; $b = 4;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
178:170:696:2:352:348
