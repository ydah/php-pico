--TEST--
integer arithmetic vector 005
--FILE--
<?php
$a = 96; $b = 9;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
105:87:864:6:201:192
