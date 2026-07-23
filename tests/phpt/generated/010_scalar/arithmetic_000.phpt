--TEST--
integer arithmetic vector 000
--FILE--
<?php
$a = 11; $b = 2;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
13:9:22:1:24:11
