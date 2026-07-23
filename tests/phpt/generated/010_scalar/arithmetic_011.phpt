--TEST--
integer arithmetic vector 011
--FILE--
<?php
$a = 198; $b = 22;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
220:176:4356:0:418:1584
