--TEST--
integer arithmetic vector 058
--FILE--
<?php
$a = 42; $b = 5;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
47:37:210:2:89:168
