--TEST--
integer arithmetic vector 089
--FILE--
<?php
$a = 187; $b = 7;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
194:180:1309:5:381:374
