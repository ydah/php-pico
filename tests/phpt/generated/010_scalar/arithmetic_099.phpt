--TEST--
integer arithmetic vector 099
--FILE--
<?php
$a = 166; $b = 21;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
187:145:3486:19:353:1328
