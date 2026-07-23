--TEST--
integer arithmetic vector 062
--FILE--
<?php
$a = 110; $b = 6;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
116:104:660:2:226:440
