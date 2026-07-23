--TEST--
integer arithmetic vector 017
--FILE--
<?php
$a = 109; $b = 12;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
121:97:1308:1:230:218
