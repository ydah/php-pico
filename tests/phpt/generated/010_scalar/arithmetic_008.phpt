--TEST--
integer arithmetic vector 008
--FILE--
<?php
$a = 147; $b = 4;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
151:143:588:3:298:147
