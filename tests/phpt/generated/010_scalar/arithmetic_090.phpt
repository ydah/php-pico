--TEST--
integer arithmetic vector 090
--FILE--
<?php
$a = 13; $b = 13;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
26:0:169:0:39:52
