--TEST--
integer arithmetic vector 068
--FILE--
<?php
$a = 21; $b = 19;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
40:2:399:2:61:21
