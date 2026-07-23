--TEST--
integer arithmetic vector 084
--FILE--
<?php
$a = 102; $b = 23;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
125:79:2346:10:227:102
