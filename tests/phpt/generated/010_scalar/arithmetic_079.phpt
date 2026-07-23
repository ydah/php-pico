--TEST--
integer arithmetic vector 079
--FILE--
<?php
$a = 17; $b = 16;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
33:1:272:1:50:136
