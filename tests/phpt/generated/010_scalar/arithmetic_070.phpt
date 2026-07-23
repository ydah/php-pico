--TEST--
integer arithmetic vector 070
--FILE--
<?php
$a = 55; $b = 8;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
63:47:440:7:118:220
