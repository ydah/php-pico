--TEST--
integer arithmetic vector 057
--FILE--
<?php
$a = 25; $b = 22;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
47:3:550:3:72:50
