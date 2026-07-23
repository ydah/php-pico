--TEST--
integer arithmetic vector 063
--FILE--
<?php
$a = 127; $b = 12;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
139:115:1524:7:266:1016
