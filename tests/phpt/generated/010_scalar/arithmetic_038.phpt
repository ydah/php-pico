--TEST--
integer arithmetic vector 038
--FILE--
<?php
$a = 84; $b = 23;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
107:61:1932:15:191:336
