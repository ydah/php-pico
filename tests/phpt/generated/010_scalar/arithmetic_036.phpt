--TEST--
integer arithmetic vector 036
--FILE--
<?php
$a = 50; $b = 11;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
61:39:550:6:111:50
