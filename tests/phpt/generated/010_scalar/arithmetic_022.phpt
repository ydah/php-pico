--TEST--
integer arithmetic vector 022
--FILE--
<?php
$a = 194; $b = 19;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
213:175:3686:4:407:776
