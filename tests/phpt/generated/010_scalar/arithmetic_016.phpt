--TEST--
integer arithmetic vector 016
--FILE--
<?php
$a = 92; $b = 6;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
98:86:552:2:190:92
