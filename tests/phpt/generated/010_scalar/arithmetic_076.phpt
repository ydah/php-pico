--TEST--
integer arithmetic vector 076
--FILE--
<?php
$a = 157; $b = 21;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 0;
--EXPECT--
178:136:3297:10:335:157
