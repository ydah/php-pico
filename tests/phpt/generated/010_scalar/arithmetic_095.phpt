--TEST--
integer arithmetic vector 095
--FILE--
<?php
$a = 98; $b = 20;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
118:78:1960:18:216:784
