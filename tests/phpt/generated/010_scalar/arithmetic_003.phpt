--TEST--
integer arithmetic vector 003
--FILE--
<?php
$a = 62; $b = 20;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
82:42:1240:2:144:496
