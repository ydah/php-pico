--TEST--
integer arithmetic vector 075
--FILE--
<?php
$a = 140; $b = 15;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
155:125:2100:5:295:1120
