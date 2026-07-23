--TEST--
integer arithmetic vector 004
--FILE--
<?php
$a = 79; $b = 3;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
82:76:237:1:161:79
