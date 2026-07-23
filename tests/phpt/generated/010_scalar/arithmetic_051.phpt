--TEST--
integer arithmetic vector 051
--FILE--
<?php
$a = 114; $b = 9;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
123:105:1026:6:237:912
