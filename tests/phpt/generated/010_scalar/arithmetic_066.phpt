--TEST--
integer arithmetic vector 066
--FILE--
<?php
$a = 178; $b = 7;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
185:171:1246:3:363:712
