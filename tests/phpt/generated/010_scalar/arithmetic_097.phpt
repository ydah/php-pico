--TEST--
integer arithmetic vector 097
--FILE--
<?php
$a = 132; $b = 9;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
141:123:1188:6:273:264
